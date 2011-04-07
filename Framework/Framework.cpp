// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "Framework.h"
#include "Application.h"
#include "Platform.h"
#include "Foundation.h"
#include "ConfigurationManager.h"
#include "EventManager.h"
#include "ModuleManager.h"
#include "ComponentManager.h"
#include "ServiceManager.h"
#include "RenderServiceInterface.h"
#include "ConsoleServiceInterface.h"
#include "ConsoleCommandServiceInterface.h"
#include "NaaliApplication.h"
#include "CoreException.h"
#include "InputAPI.h"
#include "FrameAPI.h"
#include "AssetAPI.h"
#include "GenericAssetFactory.h"
#include "AudioAPI.h"
#include "ConsoleAPI.h"
#include "DebugAPI.h"
#include "SceneAPI.h"
#include "ConfigAPI.h"

#include "UiAPI.h"
#include "NaaliMainWindow.h"

#include "SceneManager.h"
#include "SceneEvents.h"
/*
#include <Poco/Logger.h>
#include <Poco/LoggingFactory.h>
#include <Poco/FormattingChannel.h>
#include <Poco/SplitterChannel.h>
#include <Poco/Path.h>
#include <Poco/UnicodeConverter.h>
*/
#include <QApplication>
#include <QGraphicsView>
#include <QIcon>
#include <QMetaMethod>

#include "MemoryLeakCheck.h"

namespace Foundation
{
    Framework::Framework(int argc, char** argv) :
        exit_signal_(false),
        argc_(argc),
        argv_(argv),
        initialized_(false),
        headless_(false),
        naaliApplication(0),
        frame(new FrameAPI(this)),
        console(new ConsoleAPI(this)),
        ui(0),
        input(0),
        asset(0),
        audio(0),
        debug(new DebugAPI(this)),
        scene(new SceneAPI(this))
    {
        ParseProgramOptions();

        if (commandLineVariables.count("help")) 
        {
            std::cout << "Supported command line arguments: " << std::endl;
            std::cout << commandLineDescriptions << std::endl;
        }
        else
        {
            if (commandLineVariables.count("headless"))
                headless_ = true;
#ifdef PROFILING
            ProfilerSection::SetProfiler(&profiler_);
#endif
            PROFILE(FW_Startup);
            platform_ = PlatformPtr(new Platform(this));

            // Create config manager
            config_manager_ = ConfigurationManagerPtr(new ConfigurationManager(this));
            config_manager_->DeclareSetting(Framework::ConfigurationGroup(), std::string("window_title"), std::string("realXtend Naali"));
            config_manager_->DeclareSetting(Framework::ConfigurationGroup(), std::string("log_console"), bool(true));
            config_manager_->DeclareSetting(Framework::ConfigurationGroup(), std::string("log_level"), std::string("information"));

            platform_->PrepareApplicationDataDirectory(); // depends on config

            // Force install directory as the current working directory.
            /** \Todo: we may not want to do this in all cases, but there is a huge load of places
                that depend on being able to refer to the install dir with .*/
            boost::filesystem::current_path(platform_->GetInstallDirectory());
            
            // Now set proper path for config (one that also non-privileged users can write to)
            {
                const char *CONFIG_PATH = "/configuration";

                // Create config directory
                std::string config_path = GetPlatform()->GetApplicationDataDirectory() + CONFIG_PATH;
                if (boost::filesystem::exists(config_path) == false)
                    boost::filesystem::create_directory(config_path);

                // Old XML based config manager
                config_manager_->SetPath(config_path);

                // New INI based config api using QSettings
                config = new ConfigAPI(this, QString::fromStdString(config_path));
            }

            config_manager_->Load();

            // Set config values we explicitly always want to override
            config_manager_->SetSetting(Framework::ConfigurationGroup(), std::string("version_major"), std::string("0"));
            config_manager_->SetSetting(Framework::ConfigurationGroup(), std::string("version_minor"), std::string("3.4.1"));

            CreateLoggingSystem(); // depends on config and platform

            // create managers
            module_manager_ = ModuleManagerPtr(new ModuleManager(this));
            component_manager_ = ComponentManagerPtr(new ComponentManager(this));
            service_manager_ = ServiceManagerPtr(new ServiceManager());
            event_manager_ = EventManagerPtr(new EventManager(this));

            // Register task and scene events
            scene->RegisterSceneEvents();

            naaliApplication = new NaaliApplication(this, argc_, argv_);
            initialized_ = true;

            asset = new AssetAPI(headless_);
            const char cDefaultAssetCachePath[] = "/assetcache";
            asset->OpenAssetCache((GetPlatform()->GetApplicationDataDirectory() + cDefaultAssetCachePath).c_str());

            ui = new UiAPI(this);

            // Connect signal if main window was created. Not in headless mode.
            if (ui->MainWindow())
                connect(ui->MainWindow(), SIGNAL(WindowCloseEvent()), this, SLOT(Exit()));

            audio = new AudioAPI(asset); // Audio API depends on the Asset API, so must be loaded after Asset API is.
            asset->RegisterAssetTypeFactory(AssetTypeFactoryPtr(new GenericAssetFactory<AudioAsset>("Audio"))); ///< \todo This line needs to be removed.

            input = new InputAPI(this);

            // Initialize SceneAPI.
            scene->Initialise();

            RegisterDynamicObject("ui", ui);
            RegisterDynamicObject("frame", frame);
            RegisterDynamicObject("input", input);
            RegisterDynamicObject("console", console);
            RegisterDynamicObject("asset", asset);
            RegisterDynamicObject("audio", audio);
            RegisterDynamicObject("debug", debug);
            RegisterDynamicObject("application", naaliApplication);

            /** \todo JS now registers 'scene' manually to the default scene. Add this maybe later
                or register additiona 'sceneapi' */
            //RegisterDynamicObject("sceneapi", scene);
        }
    }

    Framework::~Framework()
    {
        event_manager_.reset();
        service_manager_.reset();
        component_manager_.reset();
        module_manager_.reset();
        config_manager_.reset();
        platform_.reset();
        application_.reset();
/*
        Poco::Logger::shutdown();

        for(size_t i=0 ; i<log_channels_.size() ; ++i)
            log_channels_[i]->release();

        log_channels_.clear();
        if (log_formatter_)
            log_formatter_->release();
*/
        delete frame;
        delete console;
        delete input;
        delete asset;
        delete ui;
        delete audio;

        // This delete must be the last one in Framework since naaliApplication derives QApplication.
        // When we delete QApplication, we must have ensured that all QObjects have been deleted.
        ///\bug Framework is itself a QObject and we should delete naaliApplication only after Framework has been deleted. A refactor is required.
        delete naaliApplication;
    }

    void Framework::CreateLoggingSystem()
    {
        /*
        PROFILE(FW_CreateLoggingSystem);
        Poco::LoggingFactory *loggingfactory = new Poco::LoggingFactory();

        Poco::Channel *consolechannel = 0;
        if (config_manager_->GetSetting<bool>(Framework::ConfigurationGroup(), "log_console"))
            consolechannel = loggingfactory->createChannel("ConsoleChannel");

        Poco::Channel *filechannel = loggingfactory->createChannel("FileChannel");
        
        std::string logfilepath = platform_->GetUserDocumentsDirectory();
        logfilepath += "/" + std::string(APPLICATION_NAME) + ".log";

        filechannel->setProperty("path",logfilepath);
        filechannel->setProperty("rotation","3M");
        filechannel->setProperty("archive","number");
        filechannel->setProperty("compress","false");

        splitterchannel = new Poco::SplitterChannel();
        if (consolechannel)
            splitterchannel->addChannel(consolechannel);
        splitterchannel->addChannel(filechannel); 

        log_formatter_ = loggingfactory->createFormatter("PatternFormatter");
        log_formatter_->setProperty("pattern","%H:%M:%S [%s] %t");
        log_formatter_->setProperty("times","local");
        Poco::Channel *formatchannel = new Poco::FormattingChannel(log_formatter_,splitterchannel);

        try
        {
#ifdef _DEBUG
            int loggingLevel = Poco::Message::PRIO_TRACE;
#else
            int loggingLevel = Poco::Message::PRIO_INFORMATION;
#endif            

            Poco::Logger::create("",formatchannel,loggingLevel);
            Poco::Logger::create("Foundation",Poco::Logger::root().getChannel(), loggingLevel);
        }
        catch(Poco::ExistsException &)
        {
            assert (false && "Somewhere, a message is pushed to log before the logger is initialized.");
        }

        try
        {
            LogInfo("Log file opened on " + GetLocalDateTimeString());
        }
        catch(Poco::OpenFileException &)
        {
            // Do not create the log file.
            splitterchannel->removeChannel(filechannel);
            LogInfo("Poco::OpenFileException. Log file not created.");
        }
#ifndef _DEBUG
        // make it so debug messages are not logged in release mode
        std::string log_level = config_manager_->GetSetting<std::string>(Framework::ConfigurationGroup(), "log_level");
        Poco::Logger::get("Foundation").setLevel(log_level);
#endif
        if (consolechannel)
            log_channels_.push_back(consolechannel);
        log_channels_.push_back(filechannel);
        log_channels_.push_back(splitterchannel);
        log_channels_.push_back(formatchannel);

        SAFE_DELETE(loggingfactory);
        */
    }

    void Framework::AddLogChannel(Poco::Channel *channel)
    {
//        assert (channel);
 //       splitterchannel->addChannel(channel);
    }

    void Framework::RemoveLogChannel(Poco::Channel *channel)
    {
 //       assert (channel);
 //       splitterchannel->removeChannel(channel);
    }

    void Framework::ParseProgramOptions()
    {
        namespace po = boost::program_options;

        ///\todo We cannot specify all commands here, since it is not extensible. Generate a method for modules to specify their own options (probably
        /// best is to have them parse their own options).
        commandLineDescriptions.add_options()
            ("headless", "Run in headless mode without any windows or rendering") // Framework & OgreRenderingModule
            ("help", "Produce help message") // Framework
            ("startserver", po::value<int>(0), "Start server automatically in specified port") // TundraLogicModule
            ("protocol", po::value<std::string>(), "Spesifies which transport layer to use. Used when starting a server and when client connects. Options: '--protocol tcp' and '--protocol udp'. Defaults to tcp if no protocol is spesified.") // KristalliProtocolModule
            ("fpslimit", po::value<float>(0), "Specifies the fps cap to use in rendering. Default: 60. Pass in 0 to disable") // OgreRenderingModule
            ("run", po::value<std::string>(), "Run script on startup") // JavaScriptModule
            ("file", po::value<std::string>(), "Load scene on startup. Accepts absolute and relative paths, local:// and http:// are accepted and fetched via the AssetAPI.") // TundraLogicModule & AssetModule
            ("storage", po::value<std::string>(), "Adds the given directory as a local storage directory on startup") // AssetModule
            ("login", po::value<std::string>(), "Automatically login to server using provided data. Url syntax: {tundra|http|https}://host[:port]/?username=x[&password=y&avatarurl=z&protocol={udp|tcp}]. Minimum information needed to try a connection in the url are host and username")
            ///\todo The following options seem to be unused in the system. These should be removed or reimplemented. -jj.
            ("user", po::value<std::string>(), "OpenSim login name")
            ("passwd", po::value<std::string>(), "OpenSim login password")
            ("server", po::value<std::string>(), "World server and port")
            ("auth_server", po::value<std::string>(), "RealXtend authentication server address and port")
            ("auth_login", po::value<std::string>(), "RealXtend authentication server user name");
        try
        {
            po::store(po::command_line_parser(argc_, argv_).options(commandLineDescriptions).allow_unregistered().run(), commandLineVariables);
        }
        catch(std::exception &e)
        {
            LogWarning(e.what());
        }
        po::notify(commandLineVariables);
    }

    void Framework::PostInitialize()
    {
        PROFILE(FW_PostInitialize);
        event_manager_->RegisterEventCategory("Framework");

        srand(time(0));

        LoadModules();

        // PostInitialize SceneAPI.
        scene->PostInitialize();

        // commands must be registered after modules are loaded and initialized
        RegisterConsoleCommands();
    }

    void Framework::ProcessOneFrame()
    {
        static tick_t clock_freq;
        static tick_t last_clocktime;

        if (!last_clocktime)
            last_clocktime = GetCurrentClockTime();

        if (!clock_freq)
            clock_freq = GetCurrentClockFreq();

        if (exit_signal_ == true)
            return; // We've accidentally ended up to update a frame, but we're actually quitting.

        {
            PROFILE(FW_MainLoop);

            tick_t curr_clocktime = GetCurrentClockTime();
            double frametime = ((double)curr_clocktime - (double)last_clocktime) / (double) clock_freq;
            last_clocktime = curr_clocktime;

            {
                PROFILE(FW_UpdateModules);
                module_manager_->UpdateModules(frametime);
            }

            // process delayed events
            {
                PROFILE(FW_ProcessDelayedEvents);
                event_manager_->ProcessDelayedEvents(frametime);
            }

            // Process the asset API updates.
            {
                PROFILE(Asset_Update);
                asset->Update(frametime);
            }

            // Process all keyboard input.
            {
                PROFILE(Input_Update);
                input->Update(frametime);
            }

            // Process all audio playback.
            {
                PROFILE(Audio_Update);
                audio->Update(frametime);
            }

            // Process frame update now. Scripts handling the frame tick will be run at this point, and will have up-to-date 
            // information after for example network updates, that have been performed by the modules.
            {
                PROFILE(Frame_Update);
                frame->Update(frametime);
            }

            // if we have a renderer service, render now
            boost::weak_ptr<Foundation::RenderServiceInterface> renderer = service_manager_->GetService<RenderServiceInterface>();
            if (renderer.expired() == false)
            {
                PROFILE(FW_Render);
                renderer.lock()->Render();
            }
        }

        RESETPROFILER
    }

    void Framework::Go()
    {
        {
            PROFILE(FW_PostInitialize);
            PostInitialize();
        }
        
        // Run our QApplication subclass NaaliApplication.
        naaliApplication->Go();

        // Qt main loop execution has ended, we are existing.
        exit_signal_ = true;

        // Reset SceneAPI.
        scene->Reset();

        // Unload modules
        UnloadModules();
    }

    void Framework::Exit()
    {
        exit_signal_ = true;
        if (naaliApplication)
            naaliApplication->AboutToExit();
    }
    
    void Framework::ForceExit()
    {
        exit_signal_ = true;
        if (naaliApplication)
            naaliApplication->quit();
    }
    
    void Framework::CancelExit()
    {
        exit_signal_ = false;
    }

    void Framework::LoadModules()
    {
        {
            PROFILE(FW_LoadModules);
            LogDebug("\n\nLOADING MODULES\n================================================================\n");
            module_manager_->LoadAvailableModules();
        }
        {
            PROFILE(FW_InitializeModules);
            LogDebug("\n\nINITIALIZING MODULES\n================================================================\n");
            module_manager_->InitializeModules();
        }
    }

    void Framework::UnloadModules()
    {
        event_manager_->ClearDelayedEvents();
        module_manager_->UninitializeModules();
        module_manager_->UnloadModules();
    }

    NaaliApplication *Framework::GetNaaliApplication() const
    {
        return naaliApplication;
    }

    Console::CommandResult Framework::ConsoleLoadModule(const StringVector &params)
    {
        if (params.size() != 2 && params.size() != 1)
            return Console::ResultInvalidParameters();

        std::string lib = params[0];
        std::string entry = params[0];
        if (params.size() == 2)
            entry = params[1];

        bool result = module_manager_->LoadModuleByName(lib, entry);

        if (!result)
            return Console::ResultFailure("Library or module not found.");

        return Console::ResultSuccess("Module " + entry + " loaded.");
    }

    Console::CommandResult Framework::ConsoleUnloadModule(const StringVector &params)
    {
        if (params.size() != 1)
            return Console::ResultInvalidParameters();

        bool result = false;
        if (module_manager_->HasModule(params[0]))
            result = module_manager_->UnloadModuleByName(params[0]);

        if (!result)
            return Console::ResultFailure("Module not found.");

        return Console::ResultSuccess("Module " + params[0] + " unloaded.");
    }

    Console::CommandResult Framework::ConsoleListModules(const StringVector &params)
    {
        boost::shared_ptr<Console::ConsoleServiceInterface> console = GetService<Console::ConsoleServiceInterface>(Service::ST_Console).lock();
        if (console)
        {
            console->Print("Loaded modules:");
            const ModuleManager::ModuleVector &modules = module_manager_->GetModuleList();
            for(size_t i = 0 ; i < modules.size() ; ++i)
                console->Print(modules[i].module_->Name());
        }

        return Console::ResultSuccess();
    }

    Console::CommandResult Framework::ConsoleSendEvent(const StringVector &params)
    {
        if (params.size() != 2)
            return Console::ResultInvalidParameters();
        
        event_category_id_t event_category = event_manager_->QueryEventCategory(params[0], false);
        if (event_category == IllegalEventCategory)
            return Console::ResultFailure("Event category not found.");
        else
        {
            event_manager_->SendEvent(event_category, ParseString<event_id_t>(params[1]), 0);
            return Console::ResultSuccess();
        }
    }

    static std::string FormatTime(double time)
    {
        char str[128];
        if (time >= 60.0)
        {
            double seconds = fmod(time, 60.0);
            int minutes = (int)(time / 60.0);
            sprintf(str, "%dmin %2.2fs", minutes, (float)seconds);
        }
        else if (time >= 1.0)
            sprintf(str, "%2.2fs", (float)time);
        else
            sprintf(str, "%2.2fms", (float)time*1000.f);

        return std::string(str);
    }

    /// Outputs a hierarchical list of all PROFILE() blocks onto the given console.
    /// @param node The root node where to start the printing.
    /// @param showUnused If true, even blocks that haven't been called will be included. If false, only
    ///        the blocks that were actually recently called are included. 
    void PrintTimingsToConsole(const Console::ConsolePtr &console, const ProfilerNodeTree *node, bool showUnused)
    {
        const ProfilerNode *timings_node = dynamic_cast<const ProfilerNode*>(node);

        // Controls whether we will recursively call self to also print all child nodes.
        bool recurseToChildren = true;

        static int level = -2;

        if (timings_node)
        {
            level += 2;
            assert (level >= 0);

            double average = timings_node->num_called_total_ == 0 ? 0.0 : timings_node->total_ / timings_node->num_called_total_;

            if (timings_node->num_called_ == 0 && !showUnused)
                recurseToChildren = false;
            else
            {
                char str[512];
                
                sprintf(str, "%s: called total: %lu, elapsed total: %s, called: %lu, elapsed: %s, avg: %s",
                    timings_node->Name().c_str(), timings_node->num_called_total_,
                    FormatTime(timings_node->total_).c_str(), timings_node->num_called_,
                    FormatTime(timings_node->elapsed_).c_str(), FormatTime(average).c_str());

    /*
                timings += timings_node->Name();
                timings += ": called total " + ToString(timings_node->num_called_total_);
                timings += ", elapsed total " + ToString(timings_node->total_);
                timings += ", called " + ToString(timings_node->num_called_);
                timings += ", elapsed " + ToString(timings_node->elapsed_);
                timings += ", average " + ToString(average);
    */

                std::string timings;
                for(int i = 0; i < level; ++i)
                    timings.append("&nbsp;");
                timings += str;

                console->Print(timings);
            }
        }

        if (recurseToChildren)
        {
            const ProfilerNodeTree::NodeList &children = node->GetChildren();
            for(ProfilerNodeTree::NodeList::const_iterator it = children.begin(); it != children.end() ; ++it)
                PrintTimingsToConsole(console, (*it).get(), showUnused);
        }

        if (timings_node)
            level -= 2;
    }

    Console::CommandResult Framework::ConsoleProfile(const StringVector &params)
    {
#ifdef PROFILING
        boost::shared_ptr<Console::ConsoleServiceInterface> console = GetService<Console::ConsoleServiceInterface>(Service::ST_Console).lock();
        if (console)
        {
            Profiler &profiler = GetProfiler();
//            ProfilerNodeTree *node = profiler.Lock().get();
            ProfilerNodeTree *node = profiler.GetRoot();
            if (params.size() > 0 && params.front() == "all")
                PrintTimingsToConsole(console, node, true);
            else
                PrintTimingsToConsole(console, node, false);
            console->Print(" ");
//            profiler.Release();
        }
#endif
        return Console::ResultSuccess();
    }

    void Framework::RegisterConsoleCommands()
    {
        boost::shared_ptr<Console::CommandService> console = GetService<Console::CommandService>(Service::ST_ConsoleCommand).lock();
        if (console)
        {
            console->RegisterCommand(Console::CreateCommand("LoadModule", 
                "Loads a module from shared library. Usage: LoadModule(lib, entry)", 
                Console::Bind(this, &Framework::ConsoleLoadModule)));

            console->RegisterCommand(Console::CreateCommand("UnloadModule", 
                "Unloads a module. Usage: UnloadModule(name)", 
                Console::Bind(this, &Framework::ConsoleUnloadModule)));

            console->RegisterCommand(Console::CreateCommand("ListModules", 
                "Lists all loaded modules.", 
                Console::Bind(this, &Framework::ConsoleListModules)));

            console->RegisterCommand(Console::CreateCommand("SendEvent", 
                "Sends an internal event. Only for events that contain no data. Usage: SendEvent(event category name, event id)", 
                Console::Bind(this, &Framework::ConsoleSendEvent)));

#ifdef PROFILING
            console->RegisterCommand(Console::CreateCommand("Profile", 
                "Outputs profiling data. Usage: Profile() for full, or Profile(name) for specific profiling block", 
                Console::Bind(this, &Framework::ConsoleProfile)));
#endif
        }
    }

#ifdef PROFILING
    Profiler &Framework::GetProfiler()
    {
        return profiler_;
    }
#endif

    ComponentManagerPtr Framework::GetComponentManager() const
    {
        return component_manager_;
    }

    ModuleManagerPtr Framework::GetModuleManager() const
    {
        return module_manager_;
    }

    ServiceManagerPtr Framework::GetServiceManager() const
    {
        return service_manager_;
    }

    EventManagerPtr Framework::GetEventManager() const
    {
        return event_manager_;
    }

    PlatformPtr Framework::GetPlatform() const
    {
        return platform_;
    }

    ConfigurationManagerPtr Framework::GetConfigManager()
    {
        return config_manager_;
    }

    ConfigurationManager &Framework::GetDefaultConfig()
    {
        return *(config_manager_.get());
    }

    ConfigurationManager *Framework::GetDefaultConfigPtr()
    {
        return config_manager_.get();
    }

    FrameAPI *Framework::Frame() const
    {
        return frame;
    }

    InputAPI *Framework::Input() const
    {
        return input;
    }

    UiAPI *Framework::Ui() const
    {
        return ui;
    }

    ConsoleAPI *Framework::Console() const
    {
        return console;
    }

    AudioAPI *Framework::Audio() const
    {
        return audio;
    }

    AssetAPI *Framework::Asset() const
    {
        return asset;
    }

    DebugAPI *Framework::Debug() const
    {
        return debug;
    }

    SceneAPI *Framework::Scene() const
    {
        return scene;
    }

    ConfigAPI *Framework::Config() const
    {
        return config;
    }

    QObject *Framework::GetModuleQObj(const QString &name)
    {
        ModuleWeakPtr module = GetModuleManager()->GetModule(name.toStdString());
        return dynamic_cast<QObject*>(module.lock().get());
    }

    bool Framework::RegisterDynamicObject(QString name, QObject *object)
    {
        if (name.length() == 0 || !object)
            return false;

        // We never override a property if it already exists.
        if (property(name.toStdString().c_str()).isValid())
            return false;

        setProperty(name.toStdString().c_str(), QVariant::fromValue<QObject*>(object));

        return true;
    }
}
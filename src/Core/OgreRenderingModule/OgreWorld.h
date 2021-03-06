// For conditions of distribution and use, see copyright notice in LICENSE

#pragma once

#include "CoreDefines.h"
#include "OgreModuleApi.h"
#include "OgreModuleFwd.h"
#include "SceneFwd.h"
#include "AssetFwd.h"
#include "Math/MathFwd.h"
#include "IRenderer.h"
#include "Color.h"

#include <QObject>
#include <QList>
#include <QPair>
#include <QHash>

#include <set>

class Framework;
class DebugLines;
class Transform;
class MeshInstanceTarget;
struct InstancingTarget;

class QRect;
class QTimer;

/// Contains the Ogre representation of a scene, ie. the Ogre Scene
class OGRE_MODULE_API OgreWorld : public QObject, public enable_shared_from_this<OgreWorld>
{
    Q_OBJECT
    Q_PROPERTY(bool drawDebugInstancing READ IsDebugInstancingEnabled WRITE SetDebugInstancingEnabled)

public:
    /// Called by the OgreRenderingModule upon the creation of a new scene
    OgreWorld(OgreRenderer::Renderer* renderer, ScenePtr scene);
    /// Fully destroys the Ogre scene
    virtual ~OgreWorld();

    /// Dynamic scene property name "ogre"
    static const char* PropertyName() { return "ogre"; }

    /// Returns an unique name to create Ogre objects that require a mandatory name. Calls the parent Renderer
    /** @param prefix Prefix for the name. */
    std::string GenerateUniqueObjectName(const std::string &prefix);

    /// Dump the debug geometry drawn this frame to the debug geometry vertex buffer. Called by Renderer before rendering.
    void FlushDebugGeometry();

    /// The default color used as ambient light for Ogre's SceneManager.
    static Color DefaultSceneAmbientLightColor();

    /// Sets scene fog to default ineffective settings, which plays nice with the SuperShader.
    /** Use this if you have altered the Ogre SceneManager's fog and want to reset it. */
    void SetDefaultSceneFog();

    /// Creates a instanced entity for mesh with materials.
    /** @param Component that will own the instanced entity/entities. Will be set as Ogre::MovalbleObject::setUserAny().
        @param Mesh asset reference. Must be loaded to the asset system.
        @param Material asset references. Each material must be loaded to the asset system. Empty refs get a default error material.
        @param Draw distance for the created instanced entities.
        @param Does the created instanced entities cast shadows.
        @return Instanced entity or null ptr if instance could not be created with given input. */
    Ogre::InstancedEntity *CreateInstance(IComponent *owner, const QString &meshRef, const AssetReferenceList &materials, float drawDistance = 0.0f, bool castShadows = false);

    /// @overload
    /** @param Component that will own the instanced entity/entities. Will be set as Ogre::MovalbleObject::setUserAny().
        @param Mesh asset. Must be in loaded state.
        @param Material asset references. Each material must be loaded to the asset system. Empty refs get a default error material.
        @param Draw distance for the created instanced entities.
        @param Does the created instanced entities cast shadows.
        @return Instanced entity or null ptr if instance could not be created with given input. */
    Ogre::InstancedEntity *CreateInstance(IComponent *owner, const AssetPtr &meshAsset, const AssetReferenceList &materials, float drawDistance = 0.0f, bool castShadows = false);

    /// Returns children for a instanced entity.
    /** In practice if you have a mesh with >1 submeshes the CreateInstance() function returned the
        parent (submesh index 0) and this function can be used to retrieve >0 submesh index instances.
        @note Do not use destroy the returned instances, use DestroyInstance() with the main instance
        returned from CreateInstance(), it will handle also child destruction. */
    QList<Ogre::InstancedEntity*> ChildInstances(Ogre::InstancedEntity *parent);

    /// Destroys instanced entities.
    /** This function must be used in pair with OgreWorld::CreateInstance as it removes the instance and its children from both internal state and from Ogre.
        The pointer given to this function can not be used after this function returns.
        @param Instanced entity to destroy. */
    void DestroyInstance(Ogre::InstancedEntity* instance);

    std::string GetUniqueObjectName(const std::string &prefix) { return GenerateUniqueObjectName(prefix); } /**< @deprecated Use GenerateUniqueObjectName @todo Add warning print */

public slots:
    /// Does a raycast into the world from screen coordinates, using specific selection layer(s)
    /** @note The coordinates are screen positions, not viewport positions [0,1].
        @param x Horizontal screen position for the origin of the ray
        @param y Vertical screen position for the origin of the ray
        @param layerMask Which selection layer(s) to use (bitmask)
        @return Raycast result structure, *never* a null pointer, use RaycastResult::entity to see if raycast hit something. */
    RaycastResult* Raycast(int x, int y, unsigned layerMask);
    RaycastResult* Raycast(const QPoint &point, unsigned layerMask) { return Raycast(point.x(), point.y(), layerMask);} /**< @overload @param point Screen point. */
    /// @overload
    /** Does a raycast into the world from screen coordinates, using all selection layers */
    RaycastResult* Raycast(int x, int y);
    RaycastResult* Raycast(const QPoint &point) { return Raycast(point.x(), point.y());} /**< @overload @param point Screen point. */
    /// @overload
    /** Does raycast into the world using a ray in world space coordinates. */
    RaycastResult* Raycast(const Ray& ray, unsigned layerMask);
    /// @overload
    /** Does a raycast into the world with specified layers and maximum distance */
    RaycastResult* Raycast(int x, int y, unsigned layerMask, float maxDistance);
    /// @overload
    /** Does a raycast into the world with specified screen point, layers and maximum distance. */
    RaycastResult* Raycast(const QPoint &point, unsigned layerMask, float maxDistance) { return Raycast(point.x(), point.y(), layerMask, maxDistance);}
    /// @overload
    /** Does a raycast into the world from screen coordinates, using all selection layers and a maximum distance */
    RaycastResult* Raycast(int x, int y, float maxDistance);
    /// @overload
    /** Does a raycast into the world with specified screen point, using all selection layers and a maximum distance */
    RaycastResult* Raycast(const QPoint &point, float maxDistance) { return Raycast(point.x(), point.y(), maxDistance);}
    /// @overload
    /** Does raycast into the world using a ray in world space coordinates and a maximum distance */
    RaycastResult* Raycast(const Ray& ray, unsigned layerMask, float maxDistance);
    
    /// Does a raycast into the world from screen coordinates using specific selection layer(s), and returns all results
    /** @note The coordinates are screen positions, not viewport positions [0,1].
        @param x Horizontal screen position for the origin of the ray
        @param y Vertical screen position for the origin of the ray
        @param layerMask Which selection layer(s) to use (bitmask)
        @return List of raycast result structure, empty if no hits. */
    QList<RaycastResult*> RaycastAll(int x, int y, unsigned layerMask);
    QList<RaycastResult*> RaycastAll(const QPoint &point, unsigned layerMask) { return RaycastAll(point.x(), point.y(), layerMask);} /**< @overload @param point Screen point. */
    /// @overload
    /** Does a raycast into the world from screen coordinates, using all selection layers, and returns all results */
    QList<RaycastResult*> RaycastAll(int x, int y);
    QList<RaycastResult*> RaycastAll(const QPoint &point) { return RaycastAll(point.x(), point.y());} /**< @overload @param point Screen point. */
    /// @overload
    /** Does raycast into the world using a ray in world space coordinates, and returns all results */
    QList<RaycastResult*> RaycastAll(const Ray& ray, unsigned layerMask);
    /// @overload
    /** Does a raycast into the world with specified layers and maximum distance, and returns all results */
    QList<RaycastResult*> RaycastAll(int x, int y, unsigned layerMask, float maxDistance);
    /// @overload
    /** Does a raycast into the world with specified screen point, layers and maximum distance and returns all results. */
    QList<RaycastResult*> RaycastAll(const QPoint &point, unsigned layerMask, float maxDistance) { return RaycastAll(point.x(), point.y(), layerMask, maxDistance);}
    /// @overload
    /** Does a raycast into the world from screen coordinates, using all selection layers and a maximum distance, and returns all results */
    QList<RaycastResult*> RaycastAll(int x, int y, float maxDistance);
    /// @overload
    /** Does a raycast into the world with specified screen point, using all selection layers and a maximum distance, and returns all results */
    QList<RaycastResult*> RaycastAll(const QPoint &point, float maxDistance) { return RaycastAll(point.x(), point.y(), maxDistance);}
    /// @overload
    /** Does raycast into the world using a ray in world space coordinates and a maximum distance, and returns all results */
    QList<RaycastResult*> RaycastAll(const Ray& ray, unsigned layerMask, float maxDistance);
    
    /// Does a frustum query to the world from viewport coordinates.
    /** @param viewRect The query rectangle in 2d window coords.
        @return List of entities within the frustrum. */
    QList<Entity*> FrustumQuery(QRect &viewRect) const;

    /// Returns whether a single entity is visible in the currently active camera
    bool IsEntityVisible(Entity* entity) const;
    
    /// Returns visible entities in the currently active camera
    QList<Entity*> VisibleEntities() const;
    
    /// Returns  whether the currently active camera is in this scene
    bool IsActive() const;
    
    /// Start tracking an entity's visibility within this scene, using any camera(s)
    /** After this, connect either to the EntityEnterView and EntityLeaveView signals,
        or the entity's EnterView & LeaveView signals, to be notified of the visibility change(s). */
    void StartViewTracking(Entity* entity);
    
    /// Stop tracking an entity's visibility
    void StopViewTracking(Entity* entity);
    
    /// Returns the Renderer instance
    OgreRenderer::Renderer* Renderer() const { return renderer_; }

    /// Returns the Ogre scene manager
    Ogre::SceneManager* OgreSceneManager() const { return sceneManager_; }

    /// Returns the parent scene
    ScenePtr Scene() const { return scene_.lock(); }

    /// Returns if instances with @c meshRef are currently in static mode.
    /** @param Mesh asset reference.
        @return True if mesh found and instancing is static, false if instancing is not static or instancing target for mesh could not be found. */
    bool IsInstancingStatic(const QString &meshRef);

    /// Sets all @c meshRef instances to static.
    /** Setting to static means all instances of this mesh ref will be immovable, even if their parent transform or placeable is moved
        they wont be updated. Advantages for static instances is significant speedup in rendering. Use this function to set static
        true for instancing enabled mesh refs that you know will not be moved by clients or scripts.
        @note Setting this will influence the current instances and any future instances with @c meshRef, but there must be at 
              least one instance when its first called for it to be applied. Typically you would call this from a script for a particular mesh ref.
        @param Mesh asset reference.
        @param If should be made static or revert previous static setting.
        @return True if instance manager could be found for the mesh ref, if not false is returned and you need to recall this function once instances exist. */
    bool SetInstancingStatic(const QString &meshRef, bool _static = true);

    /// Is debug drawing for instancing enabled.
    bool IsDebugInstancingEnabled() const;

    /// Set debug drawing for instancing  enabled.
    void SetDebugInstancingEnabled(bool enabled);

    /// Renders an axis-aligned bounding box.
    void DebugDrawAABB(const AABB &aabb, const Color &clr, bool depthTest = true);
    void DebugDrawAABB(const AABB &aabb, float r, float g, float b, bool depthTest = true) { DebugDrawAABB(aabb, Color(r, g, b), depthTest); } /**< @overload */
    /// Renders an arbitrarily oriented bounding box.
    void DebugDrawOBB(const OBB &obb, const Color &clr, bool depthTest = true);
    void DebugDrawOBB(const OBB &obb, float r, float g, float b, bool depthTest = true) { DebugDrawOBB(obb, Color(r, g, b), depthTest); } /**< @overload */
    /// Renders a line.
    void DebugDrawLine(const float3 &start, const float3 &end, const Color &clr, bool depthTest = true);
    void DebugDrawLine(const float3 &start, const float3 &end, float r, float g, float b, bool depthTest = true){ DebugDrawLine(start, end, Color(r, g, b), depthTest); } /**< @overload */
    /// Renders a plane.
    void DebugDrawPlane(const Plane &plane, const Color &clr, const float3 &refPoint = float3::zero,
        float uSpacing = 1.f, float vSpacing = 1.f,  int uSegments = 10, int vSegments = 10, bool depthTest = true);
    void DebugDrawPlane(const Plane &plane, float r, float g, float b, const float3 &refPoint = float3::zero,
        float uSpacing = 1.f, float vSpacing = 1.f, int uSegments = 10, int vSegments = 10, bool depthTest = true) { DebugDrawPlane(plane, Color(r, g, b), refPoint, uSpacing, vSpacing, uSegments, vSegments, depthTest); } /**< @overload */
    /// Renders a line segment.
    void DebugDrawLineSegment(const LineSegment &l, const Color &clr, bool depthTest = true);
    void DebugDrawLineSegment(const LineSegment &l, float r, float g, float b, bool depthTest = true) { DebugDrawLineSegment(l, Color(r, g, b), depthTest); } /**< @overload */
    /// Renders a transformation of an object.
    void DebugDrawTransform(const Transform &t, float axisLength, float boxSize, const Color &clr, bool depthTest = true);
    void DebugDrawTransform(const Transform &t, float axisLength, float boxSize, float r, float g, float b, bool depthTest = true) { DebugDrawTransform(t, axisLength, boxSize, Color(r, g, b), depthTest); } /**< @overload */
    /// Renders a transformation of an object.
    void DebugDrawFloat3x4(const float3x4 &t, float axisLength, float boxSize, const Color &clr, bool depthTest = true);
    void DebugDrawFloat3x4(const float3x4 &t, float axisLength, float boxSize, float r, float g, float b, bool depthTest = true) { DebugDrawFloat3x4(t, axisLength, boxSize, Color(r, g, b), depthTest); } /**< @overload */
    /// Renders a transform's local X, Y & Z axes in world space, with scaling
    void DebugDrawAxes(const float3x4 &t, bool depthTest = true);
    /// Renders a debug representation of a light.
    /** @param transform Transform of the light. The scale is ignored.
        @param lightType 0=point, 1=spot, 2=directional
        @param range Range of the light (point and spot lights only)
        @param spotAngle Spotlight cone outer angle in degrees (spot lights only) */
    void DebugDrawLight(const float3x4 &t, int lightType, float range, float spotAngle, const Color &clr, bool depthTest = true);
    void DebugDrawLight(const float3x4 &t, int lightType, float range, float spotAngle, float r, float g, float b, bool depthTest = true) { DebugDrawLight(t, lightType, range, spotAngle, Color(r, g, b), depthTest); } /**< @overload */
    /// Renders a hollow circle.
    /// @param numSubdivisions The number of edges to subdivide the circle into. This value must be at least 3.
    void DebugDrawCircle(const Circle &c, int numSubdivisions, const Color &clr, bool depthTest = true);
    void DebugDrawCircle(const Circle &c, int numSubdivisions, float r, float g, float b, bool depthTest = true) { DebugDrawCircle(c, numSubdivisions, Color(r, g, b), depthTest); } /**< @overload */
    /// Renders a simple box-like debug camera.
    void DebugDrawCamera(const float3x4 &t, float size, const Color &clr, bool depthTest = true);
    void DebugDrawCamera(const float3x4 &t, float size, float r, float g, float b, bool depthTest = true) { DebugDrawCamera(t, size, Color(r, g, b), depthTest); } /**< @overload */
    /// Renders a visualization for a spatial EC_Sound object.
    void DebugDrawSoundSource(const float3 &soundPos, float soundInnerRadius, float soundOuterRadius, const Color &clr, bool depthTest = true);
    void DebugDrawSoundSource(const float3 &soundPos, float soundInnerRadius, float soundOuterRadius, float r, float g, float b, bool depthTest = true) { DebugDrawSoundSource(soundPos, soundInnerRadius, soundOuterRadius, Color(r, g, b), depthTest); } /**< @overload */
    /// Renders a sphere as geosphere.
    void DebugDrawSphere(const float3& center, float radius, int vertices, const Color &clr, bool depthTest = true);
    void DebugDrawSphere(const float3& center, float radius, int vertices, float r, float g, float b, bool depthTest = true) { DebugDrawSphere(center, radius, vertices, Color(r, g, b), depthTest); } /**< @overload */

signals:
    /// An entity has entered the view
    void EntityEnterView(Entity* entity);

    /// An entity has left the view
    void EntityLeaveView(Entity* entity);

private slots:
    /// Handle frame update. Used for entity visibility tracking
    void OnUpdated(float timeStep);

private:
    /// Do the actual raycast. rayQuery_ must have been set up beforehand
    void RaycastInternal(unsigned layerMask, float maxDistance, bool getAllResults);

    /// Clear the hit status from raycast results.
    void ClearRaycastResults();
    
    /// Get or create a new raycast result object
    RaycastResult* GetOrCreateRaycastResult(size_t index);

    /// Setup shadows
    void SetupShadows();
    
    /// Returns the currently active camera component, if it belongs to this scene. Else return null
    EC_Camera* VerifyCurrentSceneCameraComponent() const;
    
    /// Verify that the currently active camera belongs to this scene. Returns its OgreCamera, or null if mismatch
    Ogre::Camera* VerifyCurrentSceneCamera() const;
    
    /// Framework
    Framework* framework_;
    
    /// Parent renderer
    OgreRenderer::Renderer* renderer_;
    
    /// Parent scene
    SceneWeakPtr scene_;
    
    /// Ogre scenemanager
    Ogre::SceneManager* sceneManager_;
    
    /// Ray for raycasting, reusable
    Ogre::RaySceneQuery *rayQuery_;
    
    /// All ray query results
    std::vector<RaycastResult*> rayResults_;
    
    /// Ray query results which contain a hit (RaycastAll only)
    QList<RaycastResult*> rayHits_;
    
    /// Soft shadow gaussian listeners
    std::list<GaussianListener *> gaussianListeners_;
    
    /// Visible entity ID's during this frame. Acquired from the active camera. Not updated if no entities are tracked for visibility.
    std::set<entity_id_t> visibleEntities_;
    
    /// Visible entity ID's during last frame. Acquired from the active camera. Not updated if no entities are tracked for visibility.
    std::set<entity_id_t> lastVisibleEntities_;
    
    /// Entities being tracked for visibility changes
    std::vector<EntityWeakPtr> visibilityTrackedEntities_;
    
    /// Debug geometry object
    DebugLines* debugLines_;

    /// Debug geometry object, no depth testing
    DebugLines* debugLinesNoDepth_;

    /// Ogre instancing data.
    QList<MeshInstanceTarget*> instancingTargets_;

    /// Debug drawing for instancing.
    bool drawDebugInstancing_;

    /// Get or create a instance manager for mesh ref and submesh index.
    /** @note meshRef needs to be a Ogre mesh resource name, not Tundra AssetAPI reference. */
    MeshInstanceTarget *GetOrCreateInstanceMeshTarget(const QString &meshRef, int submesh);

    /// Analyzes the current scene on how many instances potentially can be created with input mesh ref.
    /** @note meshRef needs to be a Ogre mesh resource name, not Tundra AssetAPI reference. */
    uint MeshInstanceCount(const QString &meshRef);

    /// Prepares a material for instanced use. This function will clone the material if necessary.
    QString PrepareInstancingMaterial(OgreMaterialAsset *material);
};

/// Instancing mesh target data.
class MeshInstanceTarget : public QObject
{
Q_OBJECT

public:
    MeshInstanceTarget(const QString &_ref, uint _batchSize, bool _static = false);
    ~MeshInstanceTarget();

    QString ref;
    uint batchSize;
    bool isStatic;

    struct ManagerTarget
    {
        ManagerTarget(int _submesh) : submesh(_submesh), changed(false), manager(0) {}
        bool changed; ///< Changed flag for optimizations.
        int submesh; ///< Submesh index.
        Ogre::InstanceManager *manager; ///< Manager for this submesh.
        QList<QPair<Ogre::InstancedEntity*, Ogre::InstancedEntity*> > instances; ///< Pair of child to parent. Parent can be null ptr for main entities.
    };

    QList<ManagerTarget*> managers;

    /// Creates a instance with this manager.
    Ogre::InstancedEntity *CreateInstance(Ogre::SceneManager *sceneManager, int submesh, const QString &material, Ogre::InstancedEntity *parent = 0);

    /// Destroys the instance from internal state and from Ogre.
    /** @note Also destroys all children of this instance. */
    bool DestroyInstance(Ogre::SceneManager *sceneManager, Ogre::InstancedEntity *instance);

    /// Returns if this target contains an instance.
    bool Contains(const Ogre::InstancedEntity *instance);

    /// Returns all instances created with this Ogre::InstanceManager, parented or not.
    QList<Ogre::InstancedEntity*> Instances() const;

    /// Returns all parent intances created with this Ogre::InstanceManager.
    QList<Ogre::InstancedEntity*> Parents() const;

    /// Returns all children for a instanced entity.
    /** Basically these are >0 submesh index instances for a 0 submesh index parent. */
    QList<Ogre::InstancedEntity*> Children(const Ogre::InstancedEntity *parent) const;

public slots:
    void OptimizeBatches();
    void SetBatchesStatic(bool isStatic);
    void SetDebuggingEnabled(bool enabled, const QString &material = "");

private slots:
    void InvokeOptimizations(int optimizeAfterMsec = 1000);

private:
    QTimer *optimizationTimer_;
};


#pragma once

#ifndef __SCENEGRAPH_H__
#define __SCENEGRAPH_H__

// Library Includes

// Local Includes
#include "Isometric/TileMap.h"
#include "QuadTreeCell.h"

// Forward Declaration
class ActorFactory;

/** List of perspectives to render the scene at.
@remarks
	This enum allows the rendering logic to delineate between
	isometric and oblique rendering modes. 
@todo
	Could extend this to having steeper angles.
*/
enum class RenderPerspective {
	OBLIQUE,
	ISOMETRIC
};

class SceneGraph {
	// Member Functions
public:
	/** Default constructor.
	 	@param actorFactory Reference to the actor factory for creating actors.
	 	@param renderer Reference to the renderer for rendering the scene.
	 	@param tileMap The base tile map.
		@param maxActorsPerCell The maximum amount of actors allowed in a single cell before the cell subdivides.
		@param tileWidth The width of a single tile.
		@param tileHeight The height or length of a single tile.
	*/
	SceneGraph(
		ActorFactory& actorFactory, Renderer& renderer, 
		const TileMap& tileMap, 
		size_t maxActorsPerCell,
		int tileWidth, int tileHeight);
	
	/** Destructor. */
	~SceneGraph() = default;

	/** Update the entire scene. */
	void Update(float deltaTime);

	/** Resolve the collisions of all actors in the scene. */
	void ResolveCollisions();

	/** Render the entire scene. */
	void Render();

	/** Render all the actors in the scene. */
	void RenderActors();

	/** Render the tiles maps of the scene. */
	void RenderTileMaps();

	/** Render a specific actor. */
	void RenderActor(Actor* pActor) const;

	/** Spawn an actor of a specified actor type.
		@param args Arguments to be forwarded to the constructor of the actor.
		@return Pointer to the newly created actor.
	*/
	template<typename ActorType, typename... Ts>
	ActorType* SpawnActor(const std::string& jsonResource, Ts&&... args);

	/** Spawn an actor of a specified actor type.
		@param actorXmlFilename The filename and path of the xml resource.
		@return Pointer to the newly created actor.
	*/
	Actor* SpawnActor(std::string actorXmlFilename);

	/** Spawn an actor of a specified actor type.
		@param actorXmlFilename The filename and path of the xml resource.
		@return Pointer to the newly created actor.
	*/
	Actor* SpawnActor(const std::string& actorXmlFilename, const Point<float>& position, float elevation);

	/** Destroy all actors and remove them from the scene.
		@note Any pointer referring to any actors in the scene will become a dangling pointer if not set to null.
	*/
	void ClearActors();

	// Collision Queries

	/** Perform a raycast query and return the first hit actor.
		@param origin The start of the line segment making up the ray.
		@param end The end of the line segment making up the ray.
	 	@param actorsToIgnore List of actors to ignore in the query.
	 	@return Pointer to the first hit actor of the cast or null if there were no actors in the path of the ray.
	*/
	Actor* RaycastFirstHit(const Point<float>& origin, const Point<float>& end, const std::vector<Actor*>& actorsToIgnore);

	/** Perform a raycast query and return the first hit actor.
	 	@param origin The start of the line segment making up the ray.
	 	@param direction Direction of the line segment.
	 	@param distance Distance the line segment is casted along.
	 	@param actorsToIgnore List of actors to ignore in the query.
	 	@return Pointer to the first hit actor of the cast or null if there were no actors in the path of the ray.
	*/
	Actor* RaycastFirstHit(const Point<float>& origin, const Point<float>& direction, float distance, const std::vector<Actor*>& actorsToIgnore);

	/** Perform a raycast query and return an ordered list of all hit actors.
		@param origin The start of the line segment making up the ray.
		@param end The end of the line segment making up the ray.
		@param actorsToIgnore List of actors to ignore in the query.
	 	@return Ordered list of all hit actors from first obscuring to last obscuring.
	*/
	std::vector<Actor*> Raycast(const Point<float>& origin, const Point<float>& end, const std::vector<Actor*>& actorsToIgnore);

	/** Perform a raycast query and return an ordered list of all hit actors.
		@param origin The start of the line segment making up the ray.
		@param direction Direction of the line segment.
		@param distance Distance the line segment is casted along.
		@param actorsToIgnore List of actors to ignore in the query.
	 	@return Ordered list of all hit actors from first obscuring to last obscuring.
	*/
	std::vector<Actor*> Raycast(const Point<float>& origin, const Point<float>& direction, float distance, const std::vector<Actor*>& actorsToIgnore);

	/** Pick an actor from a screen space position.
	 	@param sPosition The position in screen space to pick from.
	 	@return Actor* The first found actor at the screen position or null if no actor.
	*/
	Actor* PickActor(const Point<>& sPosition);

	// Space Conversion Helpers

	/** Converts a world position to a position on the screen.
		@remarks
			This function takes into account the camera position and zoom amount, thus will return the true 
			screen coordinate corresponding to the world position.
	 	@param wPosition The position in world space to convert.
	 	@param wElevation The elevation of the position in world space.
		@todo Currently wElevation has not effect in this function.
	 	@return The exact position on the screen where the world coordinate is projected.
	*/
	Point<float> ToScreenPosition(const Point<float>& wPosition, float wElevation) const;

	/** Converts a position on the screen to position in world space.
		@remarks
			This function takes into account the camera position and zoom amount, thus will return the true
			world position corresponding to the screen position.
		@param sPosition The position in screen space to convert.
		@return The exact position in the world where the screen coordinate projects back to.
	*/
	Point<float> ToWorldPosition(const Point<>& sPosition) const;

	/** Converts a coordinate from isometric (or oblique) space to cartesian.
		@remarks
			This function only takes into account the render perspective. It performs a simple conversion from
			isometric to cartesian coordinates. It doesn't take into account the camera's position or the
			current zoom in amount. For that @see ToScreenPosition().
		@par
			When this function is called when the scene graph has a render perspective value of OBLIQUE it will
			convert from oblique instead of isometric.
		@todo This function should probably take in an 'elevation' in world space.
	 	@param isometricCoord The isometric (or oblique) coordinate to convert to cartesian.
	*/
	Point<float> ToCartesianCoord(const Point<float>& isometricCoord) const;

	/** Convert a coordinate from cartesian space to isometric (or oblique) space.
		@remarks
			This function only takes into account the render perspective. It performs a simple conversion from 
			cartesian to isometric coordinates. It doesn't take into account the camera's position or the 
			current zoom in amount. For that @see ToWorldPosition().
		@par
			When this function is called when the scene graph has a render perspective value of OBLIQUE it will
			convert to oblique instead of isometric.
		@todo This function should probably consider an 'elevation' in world space.
		@param cartesianPosition The cartesian coordinate to convert to isometric (or oblique).
	*/
	Point<float> ToIsometricCoord(const Point<>& cartesianCoord) const;

	// Serialization

	/** Serialize the scene out to a file.
	 	@param filename Name and path to the file to be written to.
	*/
	void Serialize(const std::string& filename);

	// Accessors

	/** Add a new tile map to the back of the list of tile maps. */
	void AddTileMap(TileMap tileMap) { m_TileMaps.push_back(tileMap); }

	/** Get a tile map on a specified layer.
		@todo Could have layering within the TileMap class, thus ensuring all layers are the same size, effectively making TileMaps 3D grids.
	 	@param index Index or layer of tile maps.
	*/
	TileMap& GetTileMap(size_t index = 0);

	/** Get a tile map on a specified layer.
		@todo Could have layering within the TileMap class, thus ensuring all layers are the same size, effectively making TileMaps 3D grids.
		@param index Index or layer of tile maps.
	*/
	const TileMap& GetTileMap(size_t index = 0) const { return m_TileMaps[index]; }

	/** Set the pixel dimensions of a single tile in the world.
		@remarks 
			A single tile is considered a single unit in world space. 
			1 unit squared is equavilent to tileWidth * tileHeight.
	*/
	void SetTileDimensions(int tileWidth, int tileHeight);

	/** Get the dimensions of a single tile in pixels. */
	Point<int> GetTileDimensions() const;

	/** Set the maximum number of actors allowed in a cell of the quad tree before the cell subdivides. */
	void SetMaxNumActorsPerCell(size_t maxNumActors);

	/** Set the render perspective. */
	void SetRenderPerspective(RenderPerspective renderPerspective) { m_RenderPerspective = renderPerspective; }
	/** Get the render perspective. */
	const RenderPerspective GetRenderPerspective() const { return m_RenderPerspective; }

	/** Set the zoom. 
		@remarks The closer this value is to 0 the more zoomed out the camera is.
	*/
	void SetZoom(float zoom) { m_Zoom = zoom; }
	/** Get the zoom. */
	const float GetZoom() const { return m_Zoom; }

	/** Set the camera position.
		@remarks Recalculates the camera position in screen space.
	 	@param wCameraPosition Position of the camera in world-space.
	 	@param wCameraElevation Elevation of the camera in world-space.
	*/
	void SetCameraPosition(const Point<float>& wCameraPosition, float wCameraElevation);
	/** Get the camera position */
	const Point<float>& GetCameraPosition() const { return m_wCameraPosition; }
	/** Get the camera elevation */
	float GetCameraElevation() const { return m_wCameraElevation; }

	/** Get the tileWidth. */
	const int GetTileWidth() const { return m_TileWidth; }
	/** Get the tileHeight. */
	const int GetTileHeight() const { return m_TileHeight; }
protected:
private:
	/** Internal helper method for adding an actor to the scene. */
	Actor* AddActor(std::unique_ptr<Actor> pActor);

	/** Internal helper method for removing an actor from the scene graph.
		@return True if the actor was found and successfully removed, false if the actor doesn't exist in the scene and thus can't be removed.
	*/
	bool RemoveActor(Actor* pActor);

	/** Internal helper method for removing actors that are pending destroy. */
	void DestroyPendingActors();

	// Member Variables
public:
protected:
private:
	// Reference to the renderer.
	Renderer& m_RendererRef;

	std::vector<std::unique_ptr<Actor>> m_Actors;
	ActorFactory& m_ActorFactoryRef;

	// The map of tiles.
	//TileMap m_TileMap;
	std::vector<TileMap> m_TileMaps;

	// The width of an individual tile.
	int m_TileWidth;
	// The height of an individual tile.
	int m_TileHeight;

	// Half the width of an individual tile.
	int m_HalfTileWidth;
	// Half the height of an individual tile.
	int m_HalfTileHeight;

	// The zoom level. The higher, the more zoomed in.
	float m_Zoom;

	// The camera's world space location.
	Point<float> m_wCameraPosition;
	float m_wCameraElevation;

	Point<float> m_sCameraPosition;

	// The root of the quad tree for fast collision detection and render ordering.
	QuadTreeCell m_QuadTreeRoot;

	// The perspective to render the scene, used for calculating positions in screen space.
	RenderPerspective m_RenderPerspective;

	std::vector<std::unique_ptr<Actor>> m_NewActors;

	bool m_IsUpdatingActors = { false };
};

template<typename ActorType, typename... Ts>
ActorType* SceneGraph::SpawnActor(const std::string& jsonResource, Ts&&... args)
{
	static_assert(std::is_base_of<Actor, ActorType>::value,
		"ActorType must be derived from Actor");

	// Construct an actor of the ActorType using the given arguments.
	auto pActor = std::make_unique<ActorType>(this, std::forward<Ts>(args)...);
	m_ActorFactoryRef.AddComponentsAndInitaliseActor(pActor, jsonResource);

	assert(pActor);

	// Save a reference of the actor to return.
	auto pActorRef = pActor.get();

	AddActor(std::move(pActor));

	return pActorRef;
}

#endif	// __SCENEGRAPH_H__
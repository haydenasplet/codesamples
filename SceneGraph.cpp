
// PCH
#include "BananaFighterStd.h"

// Library Includes
#include <algorithm>
#include <fstream>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>

// This Include
#include "SceneGraph.h"

// Local Includes
#include "Utilities/JsonHelper.h"
#include "Actor/Actor.h"
#include "Actor/SpriteComponent.h"
#include "Actor/AnimationComponent.h"
#include "Actor/CollisionComponent.h"
#include "Actor/ActorFactory.h"

SceneGraph::SceneGraph(ActorFactory& actorFactory, Renderer& renderer, const TileMap& tileMap, size_t maxObjectsInCell, int tileWidth, int tileHeight)
	:m_RendererRef(renderer),
	m_ActorFactoryRef(actorFactory),
	m_QuadTreeRoot(nullptr, { -0.5f, -0.5f, static_cast<float>(tileMap.GetWidth()), static_cast<float>(tileMap.GetLength()) }, maxObjectsInCell),
	m_TileMaps({ tileMap }),
	m_Zoom(1.0f)
{
	SetTileDimensions(tileWidth, tileHeight);
	
	// Ensure m_sCameraPosition is the correct camera position in screen-space.
	SetCameraPosition(m_wCameraPosition, m_wCameraElevation);
}


void SceneGraph::Update(float deltaTime)
{
	m_IsUpdatingActors = true;

	for (const auto& pActor : m_Actors) {
		pActor->Update(deltaTime);
	}

	// Move new actors to correct list post update so that we don't invalidate our iters
	m_Actors.insert(
		m_Actors.end(),
		std::make_move_iterator(m_NewActors.begin()), 
		std::make_move_iterator(m_NewActors.end()));

	m_NewActors.clear();

	// Destroy actors that are pending destruction post update so that we don't invalidate our iters
	DestroyPendingActors();

	m_IsUpdatingActors = false;
}

void SceneGraph::ResolveCollisions()
{
	// Make sure no actors are outside the scene graph bounds.
	std::vector<Actor*> actors;
	m_QuadTreeRoot.AppendActorList(actors, false);

	const auto& rootBoundingBox = m_QuadTreeRoot.GetBoundingBox();

	for (auto actor : actors) {
		// Get the collision component of the actor.
		auto pCollisionComponent = actor->GetComponent<CollisionComponent>();
		if (pCollisionComponent) {
		// Does the actor have a collision component?
			// Loop through bounding box colliders.
			for (const auto& lBoundingBox : pCollisionComponent->GetBoundingBoxes()) {
				const auto& actorPosition = actor->GetPosition();

				// Get the world space bounding box.
				auto wBoundingBox = lBoundingBox;
				wBoundingBox.SetCentrePosition(actorPosition + lBoundingBox.GetPosition());

				if (wBoundingBox.GetLeft() < rootBoundingBox.GetLeft()) {
					wBoundingBox.SetX(rootBoundingBox.GetLeft());
				}
				if (wBoundingBox.GetRight() > rootBoundingBox.GetRight()) {
					wBoundingBox.SetX(rootBoundingBox.GetRight() - wBoundingBox.GetWidth());
				}
				if (wBoundingBox.GetTop() < rootBoundingBox.GetTop()) {
					wBoundingBox.SetY(rootBoundingBox.GetTop());
				}
				if (wBoundingBox.GetBottom() > rootBoundingBox.GetBottom()) {
					wBoundingBox.SetY(rootBoundingBox.GetBottom() - wBoundingBox.GetHeight());
				}

				// Update the player's position.
				actor->SetPosition(wBoundingBox.GetCentrePosition() - lBoundingBox.GetPosition());
			}
		}
	}

	// Resolve collisions in the tree.
	m_QuadTreeRoot.ResolveCollisions();
}

void SceneGraph::Render()
{
	RenderTileMaps();
	RenderActors();
}


void SceneGraph::RenderActors()
{
	m_QuadTreeRoot.Render(*this);
}

void SceneGraph::RenderTileMaps()
{
	auto screenCentrePosition = m_RendererRef.GetScreenCentrePosition();
	auto cullingBoxExtent = screenCentrePosition.X() / (m_TileWidth * m_Zoom) + screenCentrePosition.Y() / (m_TileHeight * m_Zoom);

	auto minX = static_cast<int>(std::roundf(m_wCameraPosition.X() - cullingBoxExtent));
	auto minY = static_cast<int>(std::roundf(m_wCameraPosition.Y() - cullingBoxExtent));
	auto maxX = static_cast<int>(std::roundf(m_wCameraPosition.X() + cullingBoxExtent)) + 1;
	auto maxY = static_cast<int>(std::roundf(m_wCameraPosition.Y() + cullingBoxExtent)) + 1;

	minX = std::max(minX, 0);
	minY = std::max(minY, 0);

	for (auto& tileMap : m_TileMaps) {
		const auto gridLength = tileMap.GetLength();
		const auto gridWidth = tileMap.GetWidth();

		if (static_cast<size_t>(minY) < gridLength && static_cast<size_t>(minX) < gridWidth) {
			// Render tile map.

			// Get the screen position of the renderable.
			auto position = Point<float>();
			auto screenPosition = ToScreenPosition(position, 0);

			auto sHalfTileWidth = m_HalfTileWidth * m_Zoom;
			auto sHalfTileHeight = m_HalfTileHeight * m_Zoom;

			// World position is the centre of the tile/object, so we must adjust 
			// the screen position to the top left corner.
			screenPosition = {
				screenPosition.X() - sHalfTileWidth,
				screenPosition.Y() - sHalfTileHeight
			};

			auto nextPosition = screenPosition;

			Point<float> xIncrease;
			Point<float> yIncrease;
			if (m_RenderPerspective == RenderPerspective::ISOMETRIC) {
				xIncrease = { sHalfTileWidth, sHalfTileHeight };
				yIncrease = { -sHalfTileWidth, sHalfTileHeight };
			}
			else if (m_RenderPerspective == RenderPerspective::OBLIQUE) {
				xIncrease = { m_TileWidth * m_Zoom, 0.0f };
				yIncrease = { 0.0f, m_TileHeight * m_Zoom };
			}

			for (size_t i = minY; i < gridLength && i < static_cast<size_t>(maxY); ++i) {
				for (size_t j = minX; j < gridWidth && j < static_cast<size_t>(maxX); ++j) {

					nextPosition = ToScreenPosition({ static_cast<float>(j), static_cast<float>(i) }, 0.0f);
					Rect<> destRect = {
						static_cast<int>(nextPosition.X() - m_HalfTileWidth * m_Zoom),
						static_cast<int>(nextPosition.Y() - m_HalfTileHeight * m_Zoom),
						static_cast<int>(std::ceil(m_TileWidth * m_Zoom)),
						static_cast<int>(std::ceil(m_TileHeight * m_Zoom))
					};

					int numLayersToRender = tileMap.GetNumLayers(j, i);
					if (tileMap.AreTransitionsHidden()) {
						// Only render base tile.
						numLayersToRender = 1;
					}

					for (int layer = 0; layer < numLayersToRender; ++layer) {
						auto& tile = tileMap.GetTile(j, i, layer);
						auto pSprite = tile.GetSpriteForRender();
						if (pSprite) {
							auto mask = tile.GetMaskForRender();

							m_RendererRef.RenderSprite(*pSprite, destRect, mask);
						}
					}
				}
			}
		}
	}
}

Actor* SceneGraph::SpawnActor(std::string actorXmlFilename)
{
	// Call overloaded method with default parameters.
	return SpawnActor(actorXmlFilename, Point<float>(), 0);
}

Actor* SceneGraph::SpawnActor(const std::string& actorXmlFilename, const Point<float>& position, float elevation)
{
	// Create the actor.
	auto pActor = std::make_unique<Actor>(this, position, elevation);
	if (!m_ActorFactoryRef.AddComponentsAndInitaliseActor(pActor, actorXmlFilename)) {
		return nullptr;
	}

	return AddActor(std::move(pActor));
}

void SceneGraph::ClearActors()
{
	for (const auto& pActor : m_Actors) {
		RemoveActor(pActor.get());
	}

	m_Actors.clear();
}

Actor* SceneGraph::RaycastFirstHit(const Point<float>& origin, const Point<float>& end, const std::vector<Actor*>& actorsToIgnore)
{
	return m_QuadTreeRoot.RaycastFirstHit(origin, end, actorsToIgnore);
}

Actor* SceneGraph::RaycastFirstHit(const Point<float>& origin, const Point<float>& direction, float distance, const std::vector<Actor*>& actorsToIgnore)
{
	return RaycastFirstHit(origin, origin + (direction * distance), actorsToIgnore);
}

std::vector<Actor*> SceneGraph::Raycast(const Point<float>& origin, const Point<float>& end, const std::vector<Actor*>& actorsToIgnore)
{
	return m_QuadTreeRoot.Raycast(origin, end, actorsToIgnore);
}

std::vector<Actor*> SceneGraph::Raycast(const Point<float>& origin, const Point<float>& direction, float distance, const std::vector<Actor*>& actorsToIgnore)
{
	return Raycast(origin, origin + (direction * distance), actorsToIgnore);
}

Actor* SceneGraph::PickActor(const Point<>& sPosition)
{
	auto wPosition = ToWorldPosition(sPosition);
	return m_QuadTreeRoot.RaycastFirstHit(wPosition, wPosition, {});
}

TileMap& SceneGraph::GetTileMap(size_t index)
{
	// Perform const cast trick to avoid code duplication. This just calls the const version of GetTileMap().
	return const_cast<TileMap&>(static_cast<const SceneGraph&>(*this).GetTileMap(index));
}

void SceneGraph::SetTileDimensions(int tileWidth, int tileHeight)
{
	m_TileWidth = tileWidth;
	m_TileHeight = tileHeight;
	m_HalfTileWidth = static_cast<int>(tileWidth / 2.0f);
	m_HalfTileHeight = static_cast<int>(tileHeight / 2.0f);
}

Point<int> SceneGraph::GetTileDimensions() const
{
	return Point<int>(m_TileHeight, m_TileWidth);
}

void SceneGraph::SetMaxNumActorsPerCell(size_t maxNumActors)
{
	m_QuadTreeRoot.SetMaxNumActors(maxNumActors, true);
}

Point<float> SceneGraph::ToScreenPosition(const Point<float>& wPosition, float wElevation) const
{
	const auto sPosition = ToCartesianCoord(wPosition);

	// Get the centre of the screen in isometric space so that the camera is always the centre of the screen.
	const auto& screenCentrePosition = m_RendererRef.GetScreenCentrePosition();
	const auto wScreenCentreOffset = Point<float>(
		static_cast<float>(screenCentrePosition.X()), 
		static_cast<float>(screenCentrePosition.Y()));

	return (sPosition - m_sCameraPosition) * m_Zoom + wScreenCentreOffset;
}

Point<float> SceneGraph::ToWorldPosition(const Point<>& sPosition) const
{
	auto wPosition = ToIsometricCoord(sPosition - m_RendererRef.GetScreenCentrePosition());

	if (m_Zoom != 0.0f) {
		wPosition /= m_Zoom;
	}
	wPosition += m_wCameraPosition;

	return wPosition;
}

Point<float> SceneGraph::ToCartesianCoord(const Point<float>& isometricCoord) const
{
	// The cartesian position.
	auto cartesianPosition = Point<float>();

	switch (m_RenderPerspective) {
		case RenderPerspective::OBLIQUE: {
			cartesianPosition = {
				isometricCoord.X() * m_TileWidth,
				isometricCoord.Y() * m_TileHeight
			};
			break;
		}
		case RenderPerspective::ISOMETRIC: {
			cartesianPosition = {
				(isometricCoord.X() - isometricCoord.Y()) * m_HalfTileWidth,
				(isometricCoord.X() + isometricCoord.Y()) * m_HalfTileHeight
			};
			break;
		}
		default: {
			break;
		}
	}

	return cartesianPosition;
}

Point<float> SceneGraph::ToIsometricCoord(const Point<>& cartesianCoord) const
{
	// The screen position.
	auto isoPosition = Point<float>();

	switch (m_RenderPerspective) {
		case RenderPerspective::OBLIQUE: {
			isoPosition = {
				cartesianCoord.X() / static_cast<float>(m_TileWidth),
				cartesianCoord.Y() / static_cast<float>(m_TileHeight)
			};
			break;
		}
		case RenderPerspective::ISOMETRIC: {
			isoPosition = {
				((cartesianCoord.Y() / static_cast<float>(m_TileHeight)) + (cartesianCoord.X() / static_cast<float>(m_TileWidth))),
				((cartesianCoord.Y() / static_cast<float>(m_TileHeight)) - (cartesianCoord.X() / static_cast<float>(m_TileWidth)))
			};
			break;
		}
		default: {
			break;
		}
	}

	return isoPosition;
}

void SceneGraph::Serialize(const std::string& filename)
{
	rapidjson::Document jsonDocument;
	jsonDocument.SetObject();

	auto& allocator = jsonDocument.GetAllocator();

	// Populate the json document.
	rapidjson::Value jsonSceneRoot(rapidjson::kObjectType);

	// Add basic scene parameters.
	jsonSceneRoot.AddMember("tile_width", m_TileWidth, allocator);
	jsonSceneRoot.AddMember("tile_height", m_TileHeight, allocator);
	jsonSceneRoot.AddMember("max_actors_per_cell", m_QuadTreeRoot.GetMaxNumActors(), allocator);

	if (m_RenderPerspective == RenderPerspective::ISOMETRIC) {
		jsonSceneRoot.AddMember("perspective", "isometric", allocator);
	}
	else {
		jsonSceneRoot.AddMember("perspective", "orthographic", allocator);
	}

	// Add bounding box.
	const auto& rootBoundingBox = m_QuadTreeRoot.GetBoundingBox();
	jsonSceneRoot.AddMember("bounding_box", JsonHelper::ToJsonValue(rootBoundingBox, allocator), allocator);

	// Add tile maps.
	rapidjson::Value jsonTileMaps(rapidjson::kArrayType);

	for (const auto& tileMap : m_TileMaps) {
		jsonTileMaps.PushBack(tileMap.Serialize(allocator), allocator);
	}

	jsonSceneRoot.AddMember("tile_maps", jsonTileMaps, allocator);

	// Add actors.
	rapidjson::Value jsonActors(rapidjson::kArrayType);

	for (const auto& pActor : m_Actors) {
		rapidjson::Value jsonActor(rapidjson::kObjectType);
		rapidjson::Value jsonResource(rapidjson::kStringType);
		jsonActor.AddMember("resource", pActor->GetResource(), allocator);
		jsonActor.AddMember("x", pActor->GetPosition().X(), allocator);
		jsonActor.AddMember("y", pActor->GetPosition().Y(), allocator);
		jsonActor.AddMember("elevation", pActor->GetElevation(), allocator);
		jsonActor.AddMember("angle", pActor->GetAngle(), allocator);

		jsonActors.PushBack(jsonActor, allocator);
	}

	jsonSceneRoot.AddMember("actors", jsonActors, allocator);
	jsonDocument.AddMember("scene", jsonSceneRoot, allocator);

	// Save the document out to file.
	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

	jsonDocument.Accept(writer);

	std::ofstream ofs(filename);
	if (ofs.is_open() && !ofs.bad()) {
		ofs << buffer.GetString();
	}
	else {
		DEBUG_ERROR() << "Failed to serialize scene. Couldn't open file \'" << filename << "\' for writing.";
	}
}

void SceneGraph::SetCameraPosition(const Point<float>& wCameraPosition, float wCameraElevation)
{
	m_wCameraPosition = wCameraPosition;
	m_wCameraElevation = wCameraElevation;

	// Update screen camera position.
	m_sCameraPosition = ToCartesianCoord(wCameraPosition);
	m_sCameraPosition = { std::floor(m_sCameraPosition.X()), std::floor(m_sCameraPosition.Y()) };
}

void SceneGraph::RenderActor(Actor* pActor) const
{
	auto spriteComponents = pActor->GetComponents<SpriteComponent>();
	auto animationComponents = pActor->GetComponents<AnimationComponent>();

	auto renderComponent = [&pActor, this](SpriteComponent* pSpriteComponent) {
		if (pSpriteComponent) {
			// Get the screen position of the actor.
			auto screenPosition = ToScreenPosition(pActor->GetPosition(), pActor->GetElevation());

			for (int i = 1; pSpriteComponent; i++) {
				auto destRect = pSpriteComponent->GetCurrentMask();
				const auto& feetOffset = pSpriteComponent->GetFeetOffset();

				destRect = {
					static_cast<int>(screenPosition.X() - (feetOffset.X() * m_Zoom)),
					static_cast<int>(screenPosition.Y() - (feetOffset.Y() * m_Zoom)),
					static_cast<int>(std::ceil(destRect.GetWidth() * m_Zoom)),
					static_cast<int>(std::ceil(destRect.GetHeight() * m_Zoom))
				};

				// Render the sprite component.
				m_RendererRef.RenderSprite(*pSpriteComponent->GetSprite(), destRect, pSpriteComponent->GetCurrentMask());

				// Next sprite component.
				pSpriteComponent = pActor->GetComponent<SpriteComponent>(i);
			}
		}
	};

	for (auto pSpriteComponent : spriteComponents) {
		renderComponent(pSpriteComponent);
	}

	for (auto pSpriteComponent : animationComponents) {
		renderComponent(pSpriteComponent);
	}
}

Actor* SceneGraph::AddActor(std::unique_ptr<Actor> pActor)
{
	auto pActorRef = pActor.get();

	if (m_IsUpdatingActors) {
		m_NewActors.push_back(std::move(pActor));
	}
	else {
		m_Actors.push_back(std::move(pActor));
	}

	m_QuadTreeRoot.InsertActor(pActorRef);
	return pActorRef;
}

bool SceneGraph::RemoveActor(Actor* pActor)
{
	auto pCell = pActor->GetQuadTreeCell();
	if (pCell) {
		return pCell->RemoveActor(pActor);
	}
	else {
		return false;
	}
}

void SceneGraph::DestroyPendingActors()
{
	auto pendingDestroy = [](const std::unique_ptr<Actor>& pActor) {
		return pActor->IsPendingDestroy();
	};

	auto removeFromGraph = [this](const std::unique_ptr<Actor>& pActor) {
		RemoveActor(pActor.get());
	};

	// Partition actors that are pending destruction.
	auto destroyBegin = std::partition(m_Actors.begin(), m_Actors.end(), pendingDestroy);

	// Remove those actors from the graph.
	std::for_each(destroyBegin, m_Actors.end(), removeFromGraph);

	// Use erase-remove idiom to remove the actors from the game.
	m_Actors.erase(destroyBegin, m_Actors.end());
}

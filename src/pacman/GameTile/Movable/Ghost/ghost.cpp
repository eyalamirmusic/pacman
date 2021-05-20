#include "ghost.hpp"

Ghost::Ghost(const std::string &_name, const sf::Vector2f &_position) {
    initVars();

    position = _position;
    name = _name;
    textures_root_dir = "res/sprites/Ghost/" + name + "/";
    resetTimers();

    initSprite();
    initTrail();
}

Ghost::Ghost(const Ghost &_ghost) { *this = _ghost; }
Ghost &Ghost::operator=(const Ghost &_other) {
    this->Movable::operator=(_other);
    chasing = _other.chasing;
    state = _other.state;
    scared_timer = _other.scared_timer;
    textures_root_dir = _other.textures_root_dir;
    name = _other.name;
    rng = _other.rng;

    return *this;
}

void Ghost::initVars() {
    Movable::initVars();

    chasing = {};
    rng = RNG();
    resetTimers();

    textures_root_dir = "res/sprites/Ghost/" + name + "/";
    // Load a default texture
    texture_paths = { EMPTY_TEXTURE };
}

void Ghost::initTrail() {
    trail.setSize({TILE_SIZE, TILE_SIZE});
    trail.setFillColor(sf::Color::Transparent);
    trail.setOutlineThickness(2.f);

    sf::Color trail_color;
    switch (name.at(0)) {
        case 'B': // Blinky
            trail_color = sf::Color::Red;
            break;
        case 'P': // Pinky
            trail_color = sf::Color(255, 192, 203); // PINK
            break;
        case 'I': // Inky
            trail_color = sf::Color::Blue;
            break;
        case 'C': // Clyde
            trail_color = sf::Color(255, 165, 0); // ORANGE
            break;
    }

    trail.setOutlineColor(trail_color);
}

void Ghost::updateAnimation() {
    std::string current_direction = "";
    
    switch (direction.x + direction.y * 10) {
        case -10: current_direction = "up"; break;
        case -1: current_direction = "left"; break;
        case 10: current_direction = "down"; break;
        case 1:
        default: current_direction = "right"; break;
    }

    texture_paths = {
        textures_root_dir + current_direction + "_1.png",
        textures_root_dir + current_direction + "_2.png"
    };

    loadTextures();
}

void Ghost::updateTrail() {
    trail_position = {
        scatter_position.x * TILE_SIZE,
        scatter_position.y * TILE_SIZE,
    };
    trail.setPosition({
        trail_position.x + X_OFFSET,
        trail_position.y + Y_OFFSET,
    });
}

void Ghost::updateMovementDirection(vec3pGT &_map) {
    PerfLogger::getInstance()->startJob("Ghost::" + name + "::updateMovementDirection");

    // Only calculate a new movement direction once the ghost has
    // fully entered into a new tile.
    const sf::Vector2i map_position = getMapPosition();
    if (position.x != map_position.x * TILE_SIZE || position.y != map_position.y * TILE_SIZE) {
        PerfLogger::getInstance()->stopJob("Ghost::" + name + "::updateMovementDirection");
        return;
    }

    // There are 2 corridors in which the ghosts are not allowed to
    // move up or down. Above the ghost house and in pacmans spawn spot.
    if ((map_position.y == 23 || map_position.y == 11) && (map_position.x > 10 && map_position.x < 17)) {
        if (direction == Directions::Left || direction == Directions::Right) {
            PerfLogger::getInstance()->stopJob("Ghost::" + name + "::updateMovementDirection");
            return;
        }
    }

    sf::Vector2i next_direction = chase(_map, getChasePosition());
    switch (state) {
        case GhostStates::Frightened:
            // Pick the next direction randomly.
            direction = frightened(_map);

            // Decrese the scared countdown.
            scared_timer--;

            // When the countdown reaches 0, the ghost
            // is no longer frightened.
            if (scared_timer <= 0) toChaseState();
            break;

        case GhostStates::Dead:
            // If the ghost has been killed, then it rushes back
            // to the ghost house
            direction = chase(_map, home_position);

            // Once the ghost house has been reached, the ghost
            // is then revived.
            if (getMapPosition() == home_position) toChaseState();
            break;

        case GhostStates::Scatter:
            // If the ghost has been killed, then it rushes back
            // to the ghost house
            direction = chase(_map, scatter_position);

            // Decrese the scared countdown.
            scatter_timer--;

            // When the countdown reaches 0, the ghost
            // is no longer scattering.
            if (scatter_timer <= 0) toChaseState();
            break;

        case GhostStates::Chase:
            // Under regular conditions, the ghost is in chase mode.
            // During chase mode, a special position is chosen for 
            // each particular ghost to chase.
            direction = chase(_map, getChasePosition());

            // Decrese the scared countdown.
            chase_timer--;

            // When the countdown reaches 0, the ghost
            // is no longer scattering.
            if (chase_timer <= 0) toChaseState();
            break;
    }


    PerfLogger::getInstance()->stopJob("Ghost::" + name + "::updateMovementDirection");
}

void Ghost::render(sf::RenderTarget *_target) const {
    _target->draw(sprite);
    _target->draw(trail);
}

void Ghost::toDeadState() {
    state = GhostStates::Dead;
    speed = SPEED;
    resetTimers();

    // Change the animation frames.
    textures_root_dir = "res/sprites/Ghost/dead/";
    loadTextures();
}

void Ghost::toChaseState() {
    state = GhostStates::Chase;
    speed = SPEED;
    resetTimers();

    // This state change turns the ghost around 180 degrees.
    direction = -direction;

    // Change the animation frames.
    textures_root_dir = "res/sprites/Ghost/" + name + "/";
    loadTextures();
}

void Ghost::toFrightenedState() {
    state = GhostStates::Frightened;
    speed = 0.5 * SPEED;
    resetTimers();
    // Change the animation frames.
    textures_root_dir = "res/sprites/Ghost/frightened/";
    loadTextures();
    updateAnimation();

    // This state change turns the ghost around 180 degrees.
    direction = -direction;
}

void Ghost::toScatterState() {
    state = GhostStates::Scatter;
    speed = SPEED;
    resetTimers();

    // This state change turns the ghost around 180 degrees.
    direction = -direction;

    // Change the animation frames.
    textures_root_dir = "res/sprites/Ghost/" + name + "/";
    loadTextures();
}

bool Ghost::canMove(vec3pGT &_map, const sf::Vector2i &_dir) const {
    PerfLogger::getInstance()->startJob("Ghost::" + name + "::canMove");

    // Ghosts cannot turn around 180 degrees
    if (direction == -_dir) {
        PerfLogger::getInstance()->stopJob("Ghost::" + name + "::canMove");
        return false;
    }

    // Get the coordinates of the vector the ghost is trying to enter.
    sf::Vector2i new_position = getMapPosition() + _dir;

    // If ANY tile within that vector is not walkable, then we abort.
    vec1pGT &vector = _map[new_position.y][new_position.x];
    for (GameTile *tile : vector) {
        if (tile != nullptr && tile->isWalkable() == false) {
            PerfLogger::getInstance()->stopJob("Ghost::" + name + "::canMove");
            return false;
        }
    }
    PerfLogger::getInstance()->stopJob("Ghost::" + name + "::canMove");
    return true;
}

sf::Vector2i Ghost::frightened(vec3pGT &_map) {
    PerfLogger::getInstance()->startJob("Ghost::" + name + "::frightened");

    // When frightened, a ghost will randomly pick a new direction
    // at every tile.
    // This is a vector that will hold all available directions
    // so that one can be picked at random.    
    std::vector< sf::Vector2i > available_directions = {};

    // Check all directions to see which are valid, and add the
    // valid ones to the vector of available directions.
    if (canMove(_map, Directions::Up)) available_directions.push_back(Directions::Up);
    if (canMove(_map, Directions::Down)) available_directions.push_back(Directions::Down);
    if (canMove(_map, Directions::Left)) available_directions.push_back(Directions::Left);
    if (canMove(_map, Directions::Right)) available_directions.push_back(Directions::Right);

    if (available_directions.empty()) {
        PerfLogger::getInstance()->stopJob("Ghost::" + name + "::frightened");
        return Directions::None;
    }

    PerfLogger::getInstance()->stopJob("Ghost::" + name + "::frightened");

    // Return the random direction from the vector
    return available_directions.at(rng.get(0, available_directions.size() - 1));
}

int Ghost::rankMove(vec3pGT &_map, const sf::Vector2i &_dest, const sf::Vector2i &_dir) const {
    PerfLogger::getInstance()->startJob("Ghost::" + name + "::rankMove");

    // Ghosts cannot turn around 180 degrees.
    if (direction == -_dir) {
        PerfLogger::getInstance()->stopJob("Ghost::" + name + "::rankMove");
        return 9999;
    }

    // Get the coordinates of the vector the ghost is trying to enter.
    sf::Vector2i new_position = getMapPosition() + _dir;
    // If ANY tile in the vector is not walkable, abort.
    vec1pGT &vector = _map[new_position.y][new_position.x];
    for (GameTile *tile : vector) {
        if (tile != nullptr && tile->isWalkable() == false) {
            PerfLogger::getInstance()->stopJob("Ghost::" + name + "::rankMove");
            return 9999;
        }
    }

    // Return the squared distance between the next tile and the destination
    const int deltaX = _dest.x - new_position.x;
    const int deltaY = _dest.y - new_position.y;

    PerfLogger::getInstance()->stopJob("Ghost::" + name + "::rankMove");
    return (deltaX * deltaX) + (deltaY * deltaY);
}

sf::Vector2i Ghost::chase(vec3pGT &_map, const sf::Vector2i &_destination) const {
    PerfLogger::getInstance()->startJob("Ghost::" + name + "::chase");

    // When chasing, a ghost will pick the available tile that gets
    // it the closest to its destination.

    // Rank each direction change in terms of distance to target.
    const int up    = rankMove(_map, _destination, Directions::Up);
    const int left  = rankMove(_map, _destination, Directions::Left);
    const int down  = rankMove(_map, _destination, Directions::Down);
    const int right = rankMove(_map, _destination, Directions::Right);
    int reference = 9999; // a number bigger than any possible distance.

    // Check each direction against the reference, to pick the best one.
    sf::Vector2i best_direction = Directions::None;
    if (up < reference) {
        reference = up;
        best_direction = Directions::Up;
    }
    if (left < reference) {
        reference = left;
        best_direction = Directions::Left;
    }
    if (down < reference) {
        reference = down;
        best_direction =Directions::Down;
    }
    if (right < reference) {
        reference = right;
        best_direction =Directions::Right;
    }

    // Return the direction that gets the ghost closest to the
    // destination tile.
    PerfLogger::getInstance()->stopJob("Ghost::" + name + "::chase");
    return best_direction;
}

void Ghost::update(const sf::RenderTarget *_target, vec3pGT &_map) {
    PerfLogger::getInstance()->startJob("Ghost::" + name + "::update");

    // Replace the Ghost tile with the tile that was previously there.
    sf::Vector2i map_pos = getMapPosition();
    vec1pGT &vector = _map[map_pos.y][map_pos.x];
    vector.erase( std::remove(vector.begin(), vector.end(), this) );

    // Update the position of the ghost.
    updateMovementDirection(_map);
    position = {
        position.x + direction.x * speed,   // X axis
        position.y + direction.y * speed    // Y axis
    };

    // Place the Ghost down onto the map at the new position.
    map_pos = getMapPosition();
    _map[map_pos.y][map_pos.x].push_back(this);

    // Update the sprite to reflect the position change.
    updateSprite();
    updateTrail();

    PerfLogger::getInstance()->stopJob("Ghost::" + name + "::update");
}
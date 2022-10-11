#include "ecs.h"

Entity_Manager::Entity_Manager() {
    for (Entity e = 0; e < max_entities; e++)
        available_entities.push(e);
}

Entity Entity_Manager::create_entity() {
    // Make sure we don't have too many
    assert(num_living_entities < max_entities);

    Entity e = available_entities.front();
    available_entities.pop();

    entities_in_use.insert(e);

    num_living_entities++;

    return e;
}

void Entity_Manager::destroy_entity(Entity e) {
    // Make sure is a valid entity handle
    assert(e >= 0 && e < max_entities);

    // Clear signature
    signatures[e].reset();

    available_entities.push(e);

    assert(entities_in_use.find(e) != entities_in_use.end());

    entities_in_use.erase(e);

    num_living_entities--;
}

void Entity_Manager::clear_entities() {
    entities_in_use.clear();

    available_entities = std::queue<Entity>();

    for (Entity e = 0; e < max_entities; e++) {
        signatures[e].reset();

        available_entities.push(e);
    }

    num_living_entities = 0;
}

void System_Manager::entity_destroyed(Entity e) {
    // Erase from all
    for (auto const &pair : systems) {
        auto const &system = pair.second;

        system->entities.erase(e);
    }
}

void System_Manager::clear_entities() {
    // Erase from all
    for (auto const &pair : systems) {
        auto const &system = pair.second;

        system->entities.clear();
    }
}

void System_Manager::entity_signature_changed(Entity e, Signature s) {
    // Notify all
    for (auto const &pair : systems) {
        auto const &id = pair.first;
        auto const &system = pair.second;
        auto const &system_signature = signatures[id];

        // If signature matches, insert into set
        if ((s & system_signature) == system_signature)
            system->entities.insert(e);
        else // Mismatch, erase
            system->entities.erase(e);
    }
}

void Coordinator::destroy_entity(Entity e) {
    entity_manager.destroy_entity(e);
    component_manager.entity_destroyed(e);
    system_manager.entity_destroyed(e);
}

void Coordinator::clear_entities() {
    entity_manager.clear_entities();
    component_manager.clear_entities();
    system_manager.clear_entities();
}

Coordinator c;

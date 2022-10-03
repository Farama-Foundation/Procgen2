#pragma once

#include <array>
#include <bitset>
#include <queue>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <assert.h>

// ECS based on https://austinmorlan.com/posts/entity_component_system/

// Handles and types
typedef int Entity;
typedef int Component_Type;

// Limits
const int max_entities = 1000;
const int max_components = 16;

// Bitset that tells us which components an entity has
typedef std::bitset<max_components> Signature;

class Entity_Manager {
private:
    std::queue<Entity> available_entities;

    std::array<Signature, max_entities> signatures;

    int num_living_entities = 0;

public:
    Entity_Manager();

    Entity create_entity();

    void destroy_entity(Entity e);

    void set_signature(Entity e, Signature s) {
        assert(e >= 0 && e < max_entities);
        
        signatures[e] = s;
    }

    Signature get_signature(Entity e) const {
        assert(e >= 0 && e < max_entities);

        return signatures[e];
    }
};

class Interface_Component_Array {
public:
    virtual ~Interface_Component_Array() = default;
    virtual void entity_destroyed(Entity e) = 0;
};

template<typename T>
class Component_Array : public Interface_Component_Array {
private:
    std::array<T, max_entities> components;

    std::unordered_map<Entity, int> entity_to_index;
    std::unordered_map<int, Entity> index_to_entity;

    int size = 0;

public:
    void insert(Entity e, T component) {
        // Make sure doesn't already exist
        assert(entity_to_index.find(e) == entity_to_index.end());

        int new_index = size;

        entity_to_index[e] = new_index;
        index_to_entity[new_index] = e;
        components[new_index] = component;

        size++;
    }

    void remove(Entity e) {
        // Make sure exists
        assert(entity_to_index.find(e) != entity_to_index.end());

        // Maintain density
        int index_removed = entity_to_index[e];
        int index_last = size - 1;

        components[index_removed] = components[index_last];

        // Update mappings
        Entity e_last = index_to_entity[index_last];
        entity_to_index[e_last] = index_removed;
        index_to_entity[index_removed] = e_last;

        entity_to_index.erase(e);
        index_to_entity.erase(index_last);

        size--;
    }

    T &get(Entity e) {
        // Make sure exists
        assert(entity_to_index.find(e) != entity_to_index.end());

        return components[entity_to_index[e]];
    }
    
    // Inherited
    void entity_destroyed(Entity e) override {
        // Remove if exists
        if (entity_to_index.find(e) != entity_to_index.end())
            remove(e);
    }
};

class Component_Manager {
private:
    std::unordered_map<size_t, Component_Type> component_types;
    std::unordered_map<size_t, std::shared_ptr<Interface_Component_Array>> component_arrays;

    Component_Type next_component_type = 0;

    template<typename T>
    std::shared_ptr<Component_Array<T>> get_component_array() {
        size_t id = typeid(T).hash_code();

        // Make sure component is registered
        assert(component_types.find(id) != component_types.end());

        return std::static_pointer_cast<Component_Array<T>>(component_arrays[id]);
    }

public:
    template<typename T>
    void register_component() {
        size_t id = typeid(T).hash_code();

        // Make sure not already registered
        assert(component_types.find(id) == component_types.end());

        component_types[id] = next_component_type;
        component_arrays[id] = std::make_shared<Component_Array<T>>();

        next_component_type++;
    }

    template<typename T>
    Component_Type get_component_type() {
        size_t id = typeid(T).hash_code();

        // Make sure exists
        assert(component_types.find(id) != component_types.end());

        return component_types[id];
    }

    template<typename T>
    void add_component(Entity e, T component) {
        get_component_array<T>()->insert(e, component);
    }

    template<typename T>
    void remove_component(Entity e) {
        get_component_array<T>()->remove(e);
    }

    template<typename T>
    T &get_component(Entity e) {
        get_component_array<T>()->get(e);
    }

    void entity_destroyed(Entity e) {
        // Notify all
        for (auto const &pair : component_arrays) {
            auto const &component = pair.second;

            component->entity_destroyed(e);
        }
    }
};

class System {
public:
    std::unordered_set<Entity> entities;
};

class System_Manager {
private:
    std::unordered_map<size_t, Signature> signatures;
    std::unordered_map<size_t, std::shared_ptr<System>> systems;

public:
    template<typename T>
    std::shared_ptr<T> register_system() {
        size_t id = typeid(T).hash_code();

        // Make sure doesn't already exist
        assert(systems.find(id) == systems.end());

        auto system = std::make_shared<T>();

        systems[id] = std::static_pointer_cast<System>(system);
        
        return system;
    }

    template<typename T>
    void set_signature(Signature s) {
        size_t id = typeid(T).hash_code();

        // Make sure exists
        assert(systems.find(id) != systems.end());

        signatures[id] = s;
    }

    void entity_destroyed(Entity e);

    void entity_signature_changed(Entity e, Signature s);
};

class Coordinator {
private:
    Entity_Manager entity_manager;
    Component_Manager component_manager;
    System_Manager system_manager;

public:
    Entity create_entity() {
        return entity_manager.create_entity();
    }

    void destroy_entity(Entity e);

    template<typename T>
    void register_component() {
        component_manager.register_component<T>();
    }

    template<typename T>
    void add_component(Entity e, T component) {
        component_manager.add_component<T>(e, component);

        auto s = entity_manager.get_signature(e);
        s.set(component_manager.get_component_type<T>(), true);
        entity_manager.set_signature(e, s);

        system_manager.entity_signature_changed(e, s);
    }

    template<typename T>
    void remove_component(Entity e) {
        component_manager.remove_component<T>(e);

        auto s = entity_manager.get_signature(e);
        s.set(component_manager.get_component_type<T>(), false);
        entity_manager.set_signature(e, s);

        system_manager.entity_signature_changed(e, s);
    }

    template<typename T>
    T &get_component(Entity e) {
        return component_manager.get_component<T>(e);
    }

    template<typename T>
    Component_Type get_component_type() {
        return component_manager.get_component_type<T>();
    }

    template<typename T>
    std::shared_ptr<T> register_system() {
        return system_manager.register_system<T>();
    }

    template<typename T>
    void set_system_signature(Signature s) {
        system_manager.set_signature<T>(s);
    }
};

static Coordinator c;

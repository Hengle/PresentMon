
#include <iostream>
#include <memory>
#include <sstream>

class Entity {

public:

    Entity(const int x, const int y) : x{ x }, y{ y } {};

    // Add pairs of ints....assume 0 for mismatched array length
    Entity add(const Entity& other) const {

        Entity r(x + other.x, y + other.y);
        return r;
    }

    Entity mul(const Entity& other) const {

        Entity r(x * other.x, y * other.y);
        return r;
    }

    Entity operator+ (const Entity& other) const {
        return add(other);
    }

    Entity operator* (const Entity& other) const {
        return mul(other);
    }

    std::string printStream() const {
        std::stringstream ss;
        ss << "Entity x: " << x << " y: " << y << " ";
        return ss.str();
    }

    // A virtual function
    virtual void vf() {};

private:
    int x, y;

};

class Player : public Entity {

public:
    Player(const std::string& name, const int x, const int y) : name(name), Entity(x, y) {};

    void setName(const std::string& name) {
        this->name = name;
    }

    std::string name;

    std::string printName() const {
        std::stringstream ss;
        ss << "Player Name: " << name << " ";
        return ss.str();
    }
};


std::ostream& operator<<(std::ostream& os, const Player& other) {
    os << other.printStream();
    os << other.printName();
    return os;
}

std::ostream& operator<<(std::ostream& os, const Entity& other) {
    os << other.printStream();
    return os;
}


void Test() {

    // Smart Pointers
    std::shared_ptr<Entity> se1 = std::make_shared<Entity>(1, 2);
    //std::cout << *se1 << std::endl;

    std::shared_ptr<Player> sp1 = std::make_shared<Player>("Babe", 100, 200);
    //std::cout << *sp1 << std::endl;

    // Try several dynamic casts
    std::shared_ptr<Entity> ret1 = std::dynamic_pointer_cast<Entity>(se1);
    std::shared_ptr<Entity> ret2 = std::dynamic_pointer_cast<Entity>(sp1);
    std::shared_ptr<Player> ret3 = std::dynamic_pointer_cast<Player>(se1);
    std::shared_ptr<Player> ret4 = std::dynamic_pointer_cast<Player>(sp1);

    std::shared_ptr<Player> ret5 = std::dynamic_pointer_cast<Player>(ret2);


    if (ret1) std::cout << "ret1 " << *ret1 << std::endl;
    if (ret2) std::cout << "ret2 " << *ret2 << std::endl;
    if (ret3) std::cout << "ret3 " << *ret3 << std::endl;
    if (ret4) std::cout << "ret4 " << *ret4 << std::endl;

    if (ret5) std::cout << "ret5 " << *ret5 << std::endl;


    std::cin.get();

}
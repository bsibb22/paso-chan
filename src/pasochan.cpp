#include "pasochan.h"

PasoChan::PasoChan(string name)
{
    //first owner
    owners.push_back(name);

    //starting params
    health = 100;
    hunger = 100;
    happiness = 50;
    stress = 40;
}

void PasoChan::add_owner(string name)
{
    //check if owner already exists
    for (int i = 0; i < owners.size(); i++)
    {
        if (owners[i] == name)
        {
            cout << name << " is already an owner" << endl;
            return;
        }
    }
    owners.push_back(name);
    cout << "Added " << name << " to owner list" << endl;
}

void PasoChan::remove_owner(string name)
{
    if (owners.size() <= 1)
    {
        cout << "Cannot remove last owner!" << endl;
        return;
    }

    bool found = false;
    for (auto it = owners.begin(); it != owners.end(); ++it)
    {
        if (*it == name)
        {
            found = true;
            owners.erase(it);
            cout << "Removed " << name << " from owner list" << endl;
            return;
        }
    }

    if (!found) 
    {
        cout << name << " is not on the owner list" << endl;
    }
}

vector<string> PasoChan::get_owners()
{
    return owners;
}

int PasoChan::get_health()
{
    return health;
}

int PasoChan::get_hunger()
{
    return hunger;
}

int PasoChan::get_happiness()
{
    return happiness;
}

int PasoChan::get_stress()
{
    return stress;
}

int PasoChan::update_health(int change)
{
    health += change;

    //check bounds
    if (health > 100) {health = 100;}
    if (health < 0) {health = 0;}

    return health;
}

int PasoChan::update_hunger(int change)
{
    hunger += change;

    //check bounds
    if (hunger > 100) {hunger = 100;}
    if (hunger < 0) {hunger = 0;}

    return hunger;
}

int PasoChan::update_happiness(int change)
{
    happiness += change;

    //check bounds
    if (happiness > 100) {happiness = 100;}
    if (happiness < 0) {happiness = 0;}

    return happiness;
}

int PasoChan::update_stress(int change)
{
    stress += change;

    //check bounds
    if (stress > 100) {stress = 100;}
    if (stress < 0) {stress = 0;}

    return stress;
}
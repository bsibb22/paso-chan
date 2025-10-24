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
    bool dupe = false;
    for (int i = 0; i < owners.size(); i++)
    {
        if (owners[i] == name)
        {
            dupe = true;
            cout << name << " is already an owner" << endl;
        }
    }
    if (!dupe)
    {
        owners.push_back(name);
        cout << "Added: " << name << " to owner list" << endl;
    }
}

void PasoChan::remover_owner(string name)
{
    for (int i = 0; i < owners.size(); i++)
    {
        if (owners[i] == name)
        {
            owners.erase(owners.begin() + i);
            cout << "Removed: " << name << " from owner list" << endl;
        }
    }
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
    return health + change;
}

int PasoChan::update_hunger(int change)
{
    return hunger + change;
}

int PasoChan::update_happiness(int change)
{
    return happiness + change;
}

int PasoChan::update_stress(int change)
{
    return stress + change;
}

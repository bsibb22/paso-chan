#include <iostream>
#include <vector>
using namespace std;

class PasoChan
{
    vector<string> owners;
    int health;
    int hunger;
    int happiness;
    int stress;

    //constructor
    PasoChan(string name);

    void add_owner(string name);
    void remover_owner(string name);

    //getters
    int get_health();
    int get_hunger();
    int get_happiness();
    int get_stress();

    //for raising or decreasing params 
    int update_health(int change);
    int update_hunger(int change);
    int update_happiness(int change);
    int update_stress(int change);
};
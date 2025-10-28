#include "pasochan.h"

int main()
{
    //create instance of class
    PasoChan paso("bmo");

    //decrease happiness
    paso.update_happiness(-25);
    cout << "Happiness: " << paso.get_happiness() << endl;

    //increase happiness
    paso.update_happiness(35);
    cout << "Happiness: " << paso.get_happiness() << endl;

    //add existing owner
    paso.add_owner("bmo");

    //remove last owner
    paso.remove_owner("bmo");

    //add new owner
    paso.add_owner("beandon");
    paso.add_owner("dome");
    paso.add_owner("jake");
    paso.add_owner("jorge");
    vector<string> owners = paso.get_owners();
    for (auto it = owners.begin(); it != owners.end(); it++)
    {
        cout << "Owners: " << *it << endl;
    }

    //remove owner not in list
    paso.remove_owner("alex");

    //remove owner
    paso.remove_owner("bmo");
    owners = paso.get_owners();
    for (int i = 0; i < owners.size(); i++)
    {
        cout << "Remaining owners: " << owners[i] << endl;
    }

    return 0;
}
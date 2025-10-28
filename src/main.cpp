#include "pasochan.h"

int main()
{
    string my_name = "bmo";
    PasoChan paso(my_name);

    cout << paso.get_happiness() << endl;
    
    return 0;
}
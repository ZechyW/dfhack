/*
 * Confiscates and dumps garbage owned by dwarfs.
 */

#include <sstream>
#include <climits>
#include <vector>
#include <set>
using namespace std;

#include <dfhack/Core.h>
#include <dfhack/Console.h>
#include <dfhack/Export.h>
#include <dfhack/PluginManager.h>
#include <vector>
#include <string>
#include <dfhack/modules/Maps.h>
#include <dfhack/modules/Items.h>
#include <dfhack/modules/Creatures.h>
#include <dfhack/modules/Materials.h>
#include <dfhack/modules/Translation.h>
using namespace DFHack;

DFhackCExport command_result df_cleanowned (Core * c, vector <string> & parameters);

DFhackCExport const char * plugin_name ( void )
{
    return "cleanowned";
}

DFhackCExport command_result plugin_init ( Core * c, std::vector <PluginCommand> &commands)
{
    commands.clear();
    commands.push_back(PluginCommand("cleanowned",
                                     "Confiscates and dumps garbage owned by dwarfs.",
                                     df_cleanowned));
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( Core * c )
{
    return CR_OK;
}

typedef std::map <DFCoord, uint32_t> coordmap;

DFhackCExport command_result df_cleanowned (Core * c, vector <string> & parameters)
{
    bool dump_scattered = false;
    bool confiscate_all = false;
    bool dry_run = false;
    int wear_dump_level = 65536;

    for(int i = 0; i < parameters.size(); i++)
    {
        string & param = parameters[i];
        if(param == "dryrun")
            dry_run = true;
        else if(param == "scattered")
            dump_scattered = true;
        else if(param == "all")
            confiscate_all = true;
        else if(param == "x")
            wear_dump_level = 1;
        else if(param == "X")
            wear_dump_level = 1;
        else if(param == "?" || param == "help")
        {
            c->con.print("Oh no! Someone has to write the help text!\n");
            return CR_OK;
        }
        else
        {
            c->con.printerr("Parameter '%s' is not valid. See 'cleanowned help'.\n",param.c_str());
            return CR_FAILURE;
        }
    }
    DFHack::Materials *Materials = c->getMaterials();
    DFHack::Items *Items = c->getItems();
    DFHack::Creatures *Creatures = c->getCreatures();
    DFHack::Translation *Tran = c->getTranslation();

    uint32_t num_creatures;
    bool ok = true;
    ok &= Materials->ReadAllMaterials();
    ok &= Creatures->Start(num_creatures);
    ok &= Tran->Start();

    vector<t_item *> p_items;
    ok &= Items->readItemVector(p_items);
    if(!ok)
    {
        c->con.printerr("Can't continue due to offset errors.\n");
        c->Resume();
        return CR_FAILURE;
    }
    c->con.print("Found total %d items.\n", p_items.size());

    for (std::size_t i=0; i < p_items.size(); i++)
    {
        t_item * curItem = p_items[i];
        DFHack::dfh_item itm;
        Items->readItem(curItem, itm);

        bool confiscate = false;
        bool dump = false;

        if (!itm.base->flags.owned)
        {
            int32_t owner = Items->getItemOwnerID(itm);
            if (owner >= 0)
            {
                c->con.print("Fixing a misflagged item: ");
                confiscate = true;
            }
            else
            {
                continue;
            }
        }

        std::string name = Items->getItemClass(itm.matdesc.itemType);

        if (itm.base->flags.rotten)
        {
            c->con.print("Confiscating a rotten item: \t");
            confiscate = true;
        }
        else if (itm.base->flags.on_ground &&
                 (name == "food" || name == "meat" || name == "plant"))
        {
            c->con.print("Confiscating a dropped foodstuff: \t");
            confiscate = true;
        }
        else if (itm.wear_level >= wear_dump_level)
        {
            c->con.print("Confiscating and dumping a worn item: \t");
            confiscate = true;
            dump = true;
        }
        else if (dump_scattered && itm.base->flags.on_ground)
        {
            c->con.print("Confiscating and dumping litter: \t");
            confiscate = true;
            dump = true;
        }
        else if (confiscate_all)
        {
            c->con.print("Confiscating: \t");
            confiscate = true;
        }

        if (confiscate)
        {
            if (!dry_run) {
                if (!Items->removeItemOwner(itm, Creatures))
                    c->con.print("(unsuccessfully) ");
                if (dump)
                    itm.base->flags.dump = 1;

                Items->writeItem(itm);
            }

            c->con.print(
                "%s (wear %d)",
                Items->getItemDescription(itm, Materials).c_str(),
                itm.wear_level
            );

            int32_t owner = Items->getItemOwnerID(itm);
            int32_t owner_index = Creatures->FindIndexById(owner);
            std::string info;

            if (owner_index >= 0)
            {
                DFHack::t_creature temp;
                Creatures->ReadCreature(owner_index,temp);
                temp.name.first_name[0] = toupper(temp.name.first_name[0]);
                info = temp.name.first_name;
                if (temp.name.nickname[0])
                    info += std::string(" '") + temp.name.nickname + "'";
                info += " ";
                info += Tran->TranslateName(temp.name,false);
                c->con.print(", owner %s", info.c_str());
            }

            c->con.print("\n");
/*
            printf(
                "%5d: %08x %08x (%d,%d,%d) #%08x [%d] %s - %s %s\n",
                i, itm.origin, itm.base.flags.whole,
                itm.base.x, itm.base.y, itm.base.z,
                itm.base.vtable,
                itm.wear_level,
                Items->getItemClass(itm.matdesc.itemType).c_str(),
                Items->getItemDescription(itm, Materials).c_str(),
                info.c_str()
                  );
                  */
        }
    }
    return CR_OK;
}

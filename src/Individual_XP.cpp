#include "ScriptMgr.h"
#include "Configuration/Config.h"
#include "ObjectMgr.h"
#include "Chat.h"
#include "Player.h"
#include "Object.h"
#include "DataMap.h"

bool IndividualXpAnnounceModule;
bool IndividualXpEnabled;

class Individual_XP_conf : public WorldScript
{
public:
    Individual_XP_conf() : WorldScript("Individual_XP_conf_conf") { }

    void OnBeforeConfigLoad(bool reload) override
    {
        IndividualXpAnnounceModule = sConfigMgr->GetBoolDefault("IndividualXp.Announce", 1);
        IndividualXpEnabled = sConfigMgr->GetBoolDefault("IndividualXp.Enabled", 1);
    }
};


class Individual_Xp_Announce : public PlayerScript
{
public:

    Individual_Xp_Announce() : PlayerScript("Individual_Xp_Announce") {}

    void OnLogin(Player* player)
    {
        // Announce Module
        if (IndividualXpEnabled & IndividualXpAnnounceModule)
        {
            ChatHandler(player->GetSession()).SendSysMessage("This server is running the |cff4CFF00IndividualXpRate |rmodule");
        }
    }
};


class PlayerXpRate : public DataMap::Base
{
public:
    PlayerXpRate() {}
    PlayerXpRate(float xpRate) : xpRate(xpRate) {}
    float xpRate = 1;
};

class Individual_XP : public PlayerScript
{
public:
    Individual_XP() : PlayerScript("Individual_XP") { }

    void OnLogin(Player* p) override
    {
        if (!IndividualXpEnabled)
        {
            return;
        }

        PreparedStatement* statement = CharacterDatabase.GetPreparedStatement(CHAR_SEL_INDIVIDUAL_XP);
        statement->setUInt32(0, p->GetGUIDLow());
        PreparedQueryResult result = CharacterDatabase.Query(statement);
        if (result)
        {
            Field* fields = result->Fetch();
            float xpRate = fields[0].GetFloat();
            if (xpRate < sWorld->getRate(RATE_XP_KILL))
            {
                p->CustomData.Set("Individual_XP", new PlayerXpRate(xpRate));
            }
        }
    }

    void OnLogout(Player* p) override
    {
        if (!IndividualXpEnabled)
        {
            return;
        }

        if (PlayerXpRate* data = p->CustomData.Get<PlayerXpRate>("Individual_XP"))
        {
            PreparedStatement* statement = CharacterDatabase.GetPreparedStatement(CHAR_UPD_INDIVIDUAL_XP);
            statement->setUInt32(0, p->GetGUIDLow());
            statement->setFloat(1, data->xpRate);
            CharacterDatabase.Execute(statement);
        }
        else
        {
            PreparedStatement* statement = CharacterDatabase.GetPreparedStatement(CHAR_DEL_INDIVIDUAL_XP);
            statement->setUInt32(0, p->GetGUIDLow());
            CharacterDatabase.Execute(statement);
        }
    }

    void OnGiveXP(Player* p, uint32& amount, Unit* victim) override
    {
        if (!IndividualXpEnabled)
        {
            return;
        }

        if (PlayerXpRate* data = p->CustomData.Get<PlayerXpRate>("Individual_XP"))
        {
            float modifier = data->xpRate / sWorld->getRate(RATE_XP_KILL);
            if (modifier < 1.0f)
            {
                amount *= modifier;
            }
        }
    }
};

class Individual_XP_command : public CommandScript
{
public:
    Individual_XP_command() : CommandScript("Individual_XP_command") { }
    std::vector<ChatCommand> GetCommands() const override
    {
        if (!IndividualXpEnabled)
        {
            return std::vector<ChatCommand>();
        }

        static std::vector<ChatCommand> IndividualXPCommandTable =
        {
            // View Command
            { "view", SEC_PLAYER, false, &HandleViewCommand, "" },
            // Set Command
            { "set", SEC_PLAYER, false, &HandleSetCommand, "" }
        };

        static std::vector<ChatCommand> IndividualXPBaseTable =
        {
            { "xp", SEC_PLAYER, false, nullptr, "", IndividualXPCommandTable }
        };

        return IndividualXPBaseTable;
    }

    // View Command
    static bool HandleViewCommand(ChatHandler* handler, char const* args)
    {
        if (*args)
            return false;

        Player* me = handler->GetSession()->GetPlayer();
        if (!me)
            return false;

        float xpRate = 1.0f;
        if (PlayerXpRate* data = me->CustomData.Get<PlayerXpRate>("Individual_XP"))
        {
            xpRate = data->xpRate;
        }
        else
        {
            xpRate = sWorld->getRate(RATE_XP_KILL);
        }

        handler->PSendSysMessage("Your current XP rate is %f", xpRate);
        return true;
    }

    // Set Command
    static bool HandleSetCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        Player* me = handler->GetSession()->GetPlayer();
        if (!me)
            return false;

        float xpRate = atof(args);
        float worldRate = sWorld->getRate(RATE_XP_KILL);

        if (xpRate < 0.0f)
        {
            xpRate = 0.0f;
        }
        if (xpRate >= worldRate)
        {
            xpRate = worldRate;
            if (PlayerXpRate* data = me->CustomData.Get<PlayerXpRate>("Individual_XP"))
            {
                me->CustomData.Erase("Individual_XP");
            }
        }
        else
        {
            if (me->CustomData.Get<PlayerXpRate>("Individual_XP") == NULL)
            {
                me->CustomData.Set("Individual_XP", new PlayerXpRate(xpRate));
            }
            else
            {
                me->CustomData.Get<PlayerXpRate>("Individual_XP")->xpRate = xpRate;
            }
        }

        handler->PSendSysMessage("Your XP rate is now %f", xpRate);
        return true;
    }
};

void AddIndividual_XPScripts()
{
    new Individual_XP_conf();
    new Individual_XP();
    new Individual_XP_command();
}

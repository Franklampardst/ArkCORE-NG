#include "throne_of_the_four_winds.h"
#include "ScriptPCH.h"
#include "Spell.h"
#include "WorldPacket.h"
#include "Vehicle.h"


enum Spells
{
    SPELL_BERSERK                       = 47008,
    SPELL_TOO_CLOSE_KNOCKBACK           = 90658,
    SPELL_STATIC_SHOCK                  = 87873,
    SPELL_SQUALL_LINE                   = 87856, // catch player
    SPELL_WIND_BURST                    = 87770,
    SPELL_LIGHTNING_STRIKE_PLAYERS      = 88214,
    SPELL_LIGHTNING_STRIKE_SINGLE       = 95764,
    SPELL_LIGHTNING_STRIKE_EFFECT       = 88238,
    SPELL_ELECTROCUTE                   = 88427,
    SPELL_FEEDBACK                      = 87904,
    SPELL_ACID_RAIN                     = 88290,
    SPELL_ACID_RAIN_REMOVE              = 91216,
    SPELL_WIND_BURST_INSTANT            = 88858,
    SPELL_RELENTLESS_STORM_VEHICLE      = 89104,
    SPELL_RELENTLESS_STORM_RING         = 88866,
    SPELL_RELENTLESS_STORM_CHANNEL      = 88875,
    SPELL_LIGHTNING                     = 89641,
    SPELL_LIGHTNING_ROD                 = 89666,
    SPELL_LIGHTNING_CLOUDS_VISUAL       = 89564,
    SPELL_LIGHTNING_CLOUDS_SINGLE_VIS   = 89583,
    SPELL_LIGHTNING_CLOUDS_DMG          = 89587,
    SPELL_LIGHTNING_CLOUDS_SUMMON       = 89565, // trigger 48190
    SPELL_LIGHTNING_CLOUDS_SUMMON_EXTRA = 89577, // trigger 48196
    SPELL_LIGHTNING_CLOUDS_SUMMON2      = 95783, // trigger 51597
    SPELL_EYE_OF_THE_STORM_TRIGGERED    = 82724,

    // Ice Storm
    SPELL_ICE_STORM_VISUAL              = 87472,
    SPELL_ICE_STORM_SUMM                = 87053,

    // Stormling
    SPELL_STORMLING_AURA                = 87905,
    SPELL_STORMLING_AURA_VISUAL         = 87906,
    SPELL_STORMLING_PRE_AURA            = 87913,
    SPELL_STORMLING_SUMMON              = 87914,
    SPELL_STORMLING_PRE_SUMMON          = 87919,
};

enum Events
{
    EVENT_MELEE_CHECK                   = 1,
    EVENT_STATIC_SHOCK,
    EVENT_SQUALL_LINE,
    EVENT_WIND_BURST,
    EVENT_ICE_STORM,
    EVENT_LIGHTNING_STRIKE,
    EVENT_STORMLING,
    EVENT_LIGHTNING,
    EVENT_LIGHTNING_ROD,
    EVENT_WIND_BURST_INSTANT,
    EVENT_LIGHTNING_CLOUDS,
    EVENT_EYE_OF_THE_STORM_REFRESH,
    EVENT_BERSERK,
    EVENT_CHECK_STORM,
};

enum PhaseAlakir
{
    PHASE_NONE,
    PHASE_ICE_STORM,
    PHASE_STORMLING,
    PHASE_WEATHER,
};

enum DataMisc
{
    DATA_ALAKIR_BUFF            = 1,
    DATA_CLOUDS_DEAL_DMG        = 2,
};

enum Timers
{
    TIMER_MELEE_CHECK           = 400,
    TIMER_STATIC_SHOCK          = 10000,
    TIMER_SQUALL_LINE           = 12000,
    TIMER_WIND_BURST            = 25000,
    TIMER_ICE_STORM             = 10000,
    TIMER_STORMLING             = 15000,
    TIMER_LIGHTNING_STRIKE      = 8000,
    TIMER_LIGHTNING             = 6000,
    TIMER_LIGHTNING_ROD         = 20000,
    TIMER_WIND_BURST_INSTANT    = 25000,
    TIMER_LIGHTNING_CLOUDS      = 15000,
    TIMER_BERSERK               = 540000
};

Position const CenterPoint = {-49.6458f, 815.082f, 191.101f, 3.12414f};

Position const SquallLinePos[2] =
{
    {-13.16094f, 780.8931f, 191.1009f, 3.566774f},
    {0.279162f,  812.344f,  191.1009f, 1.905972f},
};

class boss_alakir : public CreatureScript
{
public:
    boss_alakir() : CreatureScript("boss_alakir") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new boss_alakirAI (creature);
    }

    struct boss_alakirAI : public BossAI
    {
        boss_alakirAI(Creature* creature) : BossAI(creature, DATA_ALAKIR_EVENT)
        {
            instance = creature->GetInstanceScript();
			creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_MOVE);
        }

        InstanceScript* instance;
        PhaseAlakir phase;
        bool castedElecrocute;
        uint32 rangeCheckTimer;


        void Reset()
        {
            _Reset();
            me->AddAura(SPELL_TOO_CLOSE_KNOCKBACK, me);
            me->RemoveAurasDueToSpell(SPELL_ACID_RAIN);
            me->RemoveAurasDueToSpell(SPELL_BERSERK);
            me->StopMoving();
            phase = PHASE_NONE;
            castedElecrocute = false;
            rangeCheckTimer = 400;
            instance->SetData(DATA_ALAKIR_FLIGHT_PHASE, NOT_STARTED);
            instance->SetData(DATA_ALAKIR_EVENT, NOT_STARTED);
            instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_EYE_OF_THE_STORM_TRIGGERED);
        }

        void EnterEvadeMode()
        {
            _EnterEvadeMode();
            summons.DespawnAll();
            DespawnCreatures(NPC_STORMLING);
            DespawnCreatures(NPC_SQUILL_1);
            DespawnCreatures(NPC_SQUILL_2);
            instance->SetData(DATA_ALAKIR_FLIGHT_PHASE, FAIL);
            instance->SetData(DATA_ALAKIR_EVENT, FAIL);
            Reset();
        }

        void EnterCombat(Unit*)
        {
            _EnterCombat();
            Talk(0);

            events.Reset();
            events.ScheduleEvent(EVENT_BERSERK, TIMER_BERSERK);
            events.ScheduleEvent(EVENT_MELEE_CHECK, TIMER_MELEE_CHECK);
            events.ScheduleEvent(EVENT_STATIC_SHOCK, TIMER_STATIC_SHOCK);
            events.ScheduleEvent(EVENT_SQUALL_LINE, TIMER_SQUALL_LINE);
            events.ScheduleEvent(EVENT_WIND_BURST, TIMER_WIND_BURST);
            events.ScheduleEvent(EVENT_ICE_STORM, TIMER_ICE_STORM);
            events.ScheduleEvent(EVENT_LIGHTNING_STRIKE, TIMER_LIGHTNING_STRIKE);

            DoZoneInCombat();
            phase = PHASE_ICE_STORM;
            instance->SetData(DATA_ALAKIR_EVENT, IN_PROGRESS);
        }

        void JustDied(Unit* Killer)
        {
            _JustDied();
            Talk(7);
            summons.DespawnAll();
            instance->DoUpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET, 95673);
            instance->SetData(DATA_ALAKIR_FLIGHT_PHASE, DONE);
            instance->SetData(DATA_ALAKIR_EVENT, DONE);
        }

        void JustSummoned(Creature* summoned)
        {
            summons.Summon(summoned);
        }

        void DamageTaken(Unit* /*damageDealer*/, uint32& damage)
        {
            if (!HealthAbovePct(80) && phase == PHASE_ICE_STORM)
            {
                phase = PHASE_STORMLING;
                Talk(3);
                me->CastSpell(me, SPELL_ACID_RAIN, true);
                events.CancelEvent(EVENT_ICE_STORM);
                events.CancelEvent(EVENT_WIND_BURST);
                events.CancelEvent(EVENT_LIGHTNING_STRIKE);
                events.ScheduleEvent(EVENT_STORMLING, TIMER_STORMLING);
            }

            if (!HealthAbovePct(25) && phase == PHASE_STORMLING)
            {
                phase = PHASE_WEATHER;
                Talk(5);
                summons.DespawnAll();
                DespawnCreatures(NPC_SQUILL_1);
                DespawnCreatures(NPC_SQUILL_2);
                me->RemoveAurasDueToSpell(SPELL_TOO_CLOSE_KNOCKBACK);
                me->RemoveAurasDueToSpell(SPELL_ACID_RAIN);
                me->CastSpell(me, SPELL_ACID_RAIN_REMOVE, true);
                events.CancelEvent(EVENT_STATIC_SHOCK);
                events.CancelEvent(EVENT_SQUALL_LINE);
                events.CancelEvent(EVENT_STORMLING);

                instance->SetData(DATA_ALAKIR_FLIGHT_PHASE, IN_PROGRESS);
                me->CastSpell(me, SPELL_RELENTLESS_STORM_CHANNEL, true);

                if (Creature* trigger = me->SummonCreature(NPC_WORLD_TRIGGER, CenterPoint.GetPositionX(), CenterPoint.GetPositionY(), 190.0f, TEMPSUMMON_MANUAL_DESPAWN))
                    trigger->CastSpell(trigger, SPELL_RELENTLESS_STORM_RING, true);

                PlayersHover();
                SummonLightningClouds(true);
                events.ScheduleEvent(EVENT_LIGHTNING, TIMER_LIGHTNING);
                events.ScheduleEvent(EVENT_LIGHTNING_ROD, TIMER_LIGHTNING_ROD);
                events.ScheduleEvent(EVENT_WIND_BURST_INSTANT, TIMER_WIND_BURST_INSTANT);
                events.ScheduleEvent(EVENT_LIGHTNING_CLOUDS, TIMER_LIGHTNING_CLOUDS);
                events.ScheduleEvent(EVENT_CHECK_STORM, 1000);
            }
        }

        void DespawnCreatures(uint32 entry)
        {
            std::list<Creature*> creatures;
            GetCreatureListWithEntryInGrid(creatures, me, entry, 100.0f);

            if (creatures.empty())
                return;

            for (std::list<Creature*>::iterator iter = creatures.begin(); iter != creatures.end(); ++iter)
                (*iter)->DespawnOrUnsummon();
        }

        void PlayersHover()
        {
            Map::PlayerList const &PlayerList = me->GetMap()->GetPlayers(); 
            if (!PlayerList.isEmpty())
            {
                for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
                {
                    Player* player = i->GetSource();
                    if (player)
                    {
                        player->CastSpell(player, SPELL_EYE_OF_THE_STORM, true);
                        // if (Creature* storm = player->SummonCreature(NPC_RELENTLESS_STORM_VEHICLE, player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->GetOrientation(), TEMPSUMMON_MANUAL_DESPAWN))
                            /*player->EnterVehicle(storm)*/;
                    }
                }
            }
        }

        void PlayersGetFlyBuff()
        {
            Map* map = me->GetMap();
            Map::PlayerList const& players = map->GetPlayers();
            if (!players.isEmpty())
                for (Map::PlayerList::const_iterator i = players.begin(); i != players.end(); ++i)
                    if (Player* player = i->GetSource())
                        if (player->IsAlive())
                            player->CastSpell(player, SPELL_EYE_OF_THE_STORM, true);
        }

        void CachingStorm()
        {
            Map* map = me->GetMap();
            Map::PlayerList const& players = map->GetPlayers();
            if (players.isEmpty())
                return;

            for (Map::PlayerList::const_iterator i = players.begin(); i != players.end(); ++i)
                if (Player* player = i->GetSource())
                    if (player->IsAlive())
                        if (!player->IsInDist2d(CenterPoint.GetPositionX(), CenterPoint.GetPositionY(), 150.0f))
                            if (Creature* trigger = player->SummonCreature(NPC_ICE_STORM_ROTATE_TRIGGER, CenterPoint, TEMPSUMMON_TIMED_DESPAWN, 5000))
                            {
                                trigger->SetOrientation(trigger->GetAngle(player));
                                float x, y, z;
                                trigger->GetClosePoint(x, y, z, trigger->GetObjectSize() / 3, 80.0f);

                                if (Creature* storm = player->SummonCreature(NPC_RELENTLESS_STORM, x, y, player->GetPositionZ(), 0.0f, TEMPSUMMON_MANUAL_DESPAWN))
                                {
                                    player->CastSpell(player, SPELL_RELENTLESS_STORM_VEHICLE, true);
                                    player->EnterVehicle(storm);
                                    storm->DespawnOrUnsummon(6000);
                                }
                            }
        }

        void SummonLightningClouds(bool firstSpawn = false)
        {
            if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0, 200.0f, true))
            {
                float pos_z = target->GetPositionZ();
                if (firstSpawn)
                    pos_z = 166.7302f;

                if (Creature *centerTrigger = me->SummonCreature(NPC_LIGHTNING_CLOUD_TRIGGER, CenterPoint.GetPositionX(), CenterPoint.GetPositionY(), pos_z, target->GetOrientation(), TEMPSUMMON_TIMED_DESPAWN, 45000))
                {
                    centerTrigger->AI()->SetData(0, DATA_CLOUDS_DEAL_DMG);
                    int number = 8;
                    float intervale = (2 * M_PI) / number;
                    float x, y, z;

                    for (float distance = 30.0f; distance < 151.0f; distance += 30.0f)
                    {
                        for (int i = 1; i <= number; ++i)
                        {
                            centerTrigger->GetClosePoint(x, y, z, centerTrigger->GetObjectSize() / 3, distance, i * intervale);
                            me->SummonCreature(NPC_LIGHTNING_CLOUD_TRIGGER, x, y, z, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 45000);
                        }
                        number += 4;
                        intervale = (2 * M_PI) / number;
                    }
                }
            }
        }

        void SetData(uint32 uiI, uint32 uiValue)
        {
            if (uiValue == DATA_ALAKIR_BUFF)
            {
                me->CastSpell(me, SPELL_FEEDBACK, true);
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;

            events.Update(diff);

            if (castedElecrocute && phase != PHASE_WEATHER)
            {
                if (rangeCheckTimer <= diff)
                {
                    if (me->IsWithinMeleeRange(me->GetVictim(), 1.0f))
                    {
                        me->InterruptSpell(CURRENT_CHANNELED_SPELL);
                        castedElecrocute = false;
                    }
                    rangeCheckTimer = 400;
                }
                else rangeCheckTimer -= diff;
            }

            while(uint32 eventId = events.ExecuteEvent())
            {
                if (eventId == EVENT_BERSERK)
                {
                    me->CastSpell(me, SPELL_BERSERK, true);
                    events.CancelEvent(EVENT_BERSERK);
                }

                if (phase == PHASE_ICE_STORM || phase == PHASE_STORMLING)
                {
                    switch (eventId)
                    {
                    case EVENT_MELEE_CHECK:
                        if (me->HasUnitState(UNIT_STATE_CASTING))
                        {
                            events.RescheduleEvent(EVENT_MELEE_CHECK, 100);
                            break;
                        }
                        if (!me->IsWithinMeleeRange(me->GetVictim()))
                        {
                            castedElecrocute = true;
                            DoCastVictim(SPELL_ELECTROCUTE);
                        }
                        events.RescheduleEvent(EVENT_MELEE_CHECK, 400);
                        break;
                    case EVENT_STATIC_SHOCK:
                        me->CastSpell(me, SPELL_STATIC_SHOCK, true);
                        events.RescheduleEvent(EVENT_STATIC_SHOCK, 7000);
                        break;
                    case EVENT_SQUALL_LINE:
                        switch (urand(0, 1))
                        {
                        case 0:
                            me->SummonCreature(NPC_SQUILL_LINE_2, SquallLinePos[0], TEMPSUMMON_TIMED_DESPAWN, 40000);
                            break;
                        case 1:
                            me->SummonCreature(NPC_SQUILL_LINE_1, SquallLinePos[1], TEMPSUMMON_TIMED_DESPAWN, 40000);
                            break;
                        }
                        events.RescheduleEvent(EVENT_SQUALL_LINE, 35000);
                        break;

                    case EVENT_WIND_BURST:
                        Talk(1);
                        Talk(2);
                        me->CastSpell(me, SPELL_WIND_BURST, false);
                        events.RescheduleEvent(EVENT_WIND_BURST, 35000);
                        break;
                    case EVENT_ICE_STORM:
                        if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0, 24.0f, true))
                        {
                            me->SummonCreature(NPC_ICE_STORM_RAIN, target->GetPositionX(), target->GetPositionY(), CenterPoint.GetPositionZ(), target->GetOrientation(), TEMPSUMMON_TIMED_DESPAWN, 15000);
                        }
                        events.RescheduleEvent(EVENT_ICE_STORM, 20000);
                        break;
                    case EVENT_LIGHTNING_STRIKE:
                        if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0, 80.0f, true))
                        {
                            float orient = me->GetAngle(target);
                            me->SetOrientation(orient);
                            float x, y, z;

                            for (int i = 1; i < 17; i++)
                            {
                                me->GetClosePoint(x, y, z, me->GetObjectSize() / 10, 24.0f, i * 0.04188f);
                                if (Creature* trigger = me->SummonCreature(NPC_LIGHTNING_STRIKE_TRIGGER, x, y, z, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 4000))
                                    me->CastSpell(trigger, SPELL_LIGHTNING_STRIKE_SINGLE, true);
                            }
                            for (int i = 1; i < 17; i++)
                            {
                                me->GetClosePoint(x, y, z, me->GetObjectSize() / 10, 24.0f, -i * 0.04188f);
                                if (Creature* trigger = me->SummonCreature(NPC_LIGHTNING_STRIKE_TRIGGER, x, y, z, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 4000))
                                    me->CastSpell(trigger, SPELL_LIGHTNING_STRIKE_SINGLE, true);
                            }

                            me->CastSpell(me, SPELL_LIGHTNING_STRIKE_PLAYERS, true);
                        }
                        events.RescheduleEvent(EVENT_LIGHTNING_STRIKE, urand(8000,10000));
                        break;
                    }
                }
                if (phase == PHASE_STORMLING)
                {
                    switch (eventId)
                    {
                    case EVENT_STORMLING:
                        Talk(4);
                        if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0, 25.0f, true))
                        {
                            me->SummonCreature(NPC_STORMLING_PRE_EFFECT, target->GetPositionX(), target->GetPositionY(), target->GetPositionZ(), 0.0f, TEMPSUMMON_MANUAL_DESPAWN);
                        }
                        events.RescheduleEvent(EVENT_STORMLING, 20000);
                        break;
                    }
                }
                if (phase == PHASE_WEATHER)
                {
                    switch (eventId)
                    {
                    case EVENT_LIGHTNING:
                        if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0, 200.0f, true))
                        {
                            me->CastSpell(target, SPELL_LIGHTNING, false);
                        }
                        events.RescheduleEvent(EVENT_LIGHTNING, urand(2000, 3000));
                        break;
                    case EVENT_LIGHTNING_ROD:
                        if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0, 200.0f, true))
                        {
                            target->CastSpell(target, SPELL_LIGHTNING_ROD, true);
                        }
                        events.RescheduleEvent(EVENT_LIGHTNING_ROD, urand(15000, 20000));
                        break;
                    case EVENT_WIND_BURST_INSTANT:
                        Talk(1);
                        Talk(2);
                        me->CastSpell(me, SPELL_WIND_BURST_INSTANT, true);
                        events.RescheduleEvent(EVENT_WIND_BURST_INSTANT, 20000);
                        events.ScheduleEvent(EVENT_EYE_OF_THE_STORM_REFRESH, 1000);
                        break;
                    case EVENT_EYE_OF_THE_STORM_REFRESH:
                        PlayersGetFlyBuff();
                        events.CancelEvent(EVENT_EYE_OF_THE_STORM_REFRESH);
                        break;
                    case EVENT_LIGHTNING_CLOUDS:
                        SummonLightningClouds();
                        events.RescheduleEvent(EVENT_LIGHTNING_CLOUDS, 15000);
                        break;
                    case EVENT_CHECK_STORM:
                        CachingStorm();
                        events.RescheduleEvent(EVENT_CHECK_STORM, 1000);
                        break;
                    }
                }
            }
            if (phase != PHASE_WEATHER)
                DoMeleeAttackIfReady();
        }
    };
};

class npc_squall_line : public CreatureScript
{
public:
    npc_squall_line() : CreatureScript("npc_squall_line") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_squall_lineAI (creature);
    }

    struct npc_squall_lineAI : public ScriptedAI
    {
        npc_squall_lineAI(Creature* creature) : ScriptedAI(creature)
        {
            me->SetReactState(REACT_PASSIVE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_GRIP, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_INTERRUPT, false);
        }

        void IsSummonedBy(Unit* /*summoner*/)
        {
            if (!me->GetVehicleKit())
                return;

            uint32 tornadoEntry = 0;
            if (me->GetEntry() == NPC_SQUILL_LINE_2)
                tornadoEntry = NPC_SQUILL_2;
            else
                tornadoEntry = NPC_SQUILL_1;

            for (int i = 0; i < 8; ++i)
            {
                if (Creature* tornado = me->SummonCreature(tornadoEntry, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), me->GetOrientation(), TEMPSUMMON_TIMED_DESPAWN, 40000))
                {
                    tornado->EnterVehicle(me, i);
                }
            }

            uint8 holeWall = urand(0, 7);
            if (Unit* passenger = me->GetVehicleKit()->GetPassenger(holeWall))
            {
                passenger->ToCreature()->Kill(passenger);
                passenger->ToCreature()->DespawnOrUnsummon();
            }

            if (holeWall == 7 && tornadoEntry == NPC_SQUILL_1)
                holeWall = 6;
            else if (holeWall == 7 && tornadoEntry == NPC_SQUILL_2)
                holeWall = 5;
            else if (holeWall == 6 && tornadoEntry == NPC_SQUILL_1)
                holeWall = 7;
            else if (holeWall == 6 && tornadoEntry == NPC_SQUILL_2)
                holeWall = 4;
            else if (holeWall == 5)
                holeWall = 3;
            else
                holeWall = holeWall + 2;

            if (Unit* passenger = me->GetVehicleKit()->GetPassenger(holeWall))
            {
                passenger->ToCreature()->Kill(passenger);
                passenger->ToCreature()->DespawnOrUnsummon();
            }
        }
        void Reset()
        { }
    };
};

class npc_wind_funnel : public CreatureScript
{
public:
    npc_wind_funnel() : CreatureScript("npc_wind_funnel") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_wind_funnelAI (creature);
    }

    struct npc_wind_funnelAI : public ScriptedAI
    {
        npc_wind_funnelAI(Creature* creature) : ScriptedAI(creature)
        {
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
        }

        bool mounted;
        uint32 CheckPlayerTimer;
        uint32 DebuffRemoveTimer;

        void Reset()
        {
            mounted = false;
            CheckPlayerTimer = 5000;
            DebuffRemoveTimer = 6000;
        }

        void PassengerBoarded(Unit* who, int8 /*seatId*/, bool apply)
        {
            if (!me)
                return;

            if (apply)
            {
                mounted = true;
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (mounted)
            {
                if (DebuffRemoveTimer <= diff)
                {
                    mounted = false;
                    DebuffRemoveTimer = 6000;
                }
                else DebuffRemoveTimer -= diff;

                return;
            }

            if (CheckPlayerTimer <= diff)
            {
                if (Player* pPlayer = me->FindNearestPlayer(2.0f, true))
                {
                    if (pPlayer->HasAura(SPELL_SQUALL_LINE))
                        return;

                    pPlayer->CastSpell(me, SPELL_SQUALL_LINE, true);
                }
                CheckPlayerTimer = 500;
            }
            else CheckPlayerTimer -= diff;
        }
    };
};

// Ice Storm 46973
class npc_ice_storm : public CreatureScript
{
public:
    npc_ice_storm() : CreatureScript("npc_ice_storm") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_ice_stormAI (creature);
    }

    struct npc_ice_stormAI : public ScriptedAI
    {
        npc_ice_stormAI(Creature* creature) : ScriptedAI(creature)
        {
            me->SetReactState(REACT_PASSIVE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_GRIP, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_INTERRUPT, false);
        }

        void Reset()
        {
            me->DespawnOrUnsummon(20000);
        }

        void UpdateAI(uint32 diff) override
        { }
    };
};

// Ice Storm 46734
class npc_ice_storm_rain : public CreatureScript
{
public:
    npc_ice_storm_rain() : CreatureScript("npc_ice_storm_rain") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_ice_storm_rainAI (creature);
    }

    struct npc_ice_storm_rainAI : public ScriptedAI
    {
        npc_ice_storm_rainAI(Creature* creature) : ScriptedAI(creature)
        {
            me->SetReactState(REACT_PASSIVE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_GRIP, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_INTERRUPT, false);
        }

        float angelRain;
        float radiusRain;
        uint64 triggerGUID;
        uint32 changeAngelRainTimer;
        uint32 visualRainTimer;

        void Reset()
        {
            changeAngelRainTimer = 1;
            visualRainTimer = 1500;
            radiusRain = me->GetDistance2d(CenterPoint.GetPositionX(), CenterPoint.GetPositionY());
            if (urand(0, 1))
                angelRain = 1.0f;
            else
                angelRain = -1.0f;
            if (Creature* trigger = me->SummonCreature(NPC_ICE_STORM_ROTATE_TRIGGER, CenterPoint, TEMPSUMMON_TIMED_DESPAWN, 20000))
                triggerGUID = trigger->GetGUID();

            me->CastSpell(me, SPELL_ICE_STORM_VISUAL, true);
            me->CastSpell(me, SPELL_ICE_STORM_SUMM, true);
        }

        void RunArc()
        {
            if (Creature* trigger = ObjectAccessor::GetCreature(*me, triggerGUID))
            {
                trigger->SetOrientation(trigger->GetAngle(me));
                float orient = trigger->GetOrientation();
                trigger->SetOrientation(orient + angelRain);

                float x, y, z;
                trigger->GetClosePoint(x, y, z, me->GetObjectSize() / 3, radiusRain);

                me->GetMotionMaster()->Clear(true);
                me->GetMotionMaster()->MovePoint(0, x, y, z);
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (changeAngelRainTimer <= diff)
            {
                RunArc();
                changeAngelRainTimer = 3000;
            }
            else changeAngelRainTimer -= diff;

            if (visualRainTimer <= diff)
            {
                me->CastSpell(me, SPELL_ICE_STORM_VISUAL, true);
                visualRainTimer = 500;
            }
            else visualRainTimer -= diff;
        }
    };
};

// Stormling Pre-effect 47177
class npc_stormling_pre_effect : public CreatureScript
{
public:
    npc_stormling_pre_effect() : CreatureScript("npc_stormling_pre_effect") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_stormling_pre_effectAI (creature);
    }

    struct npc_stormling_pre_effectAI : public ScriptedAI
    {
        npc_stormling_pre_effectAI(Creature* creature) : ScriptedAI(creature)
        {
            me->SetReactState(REACT_PASSIVE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_GRIP, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_INTERRUPT, false);
        }

        uint32 summonTimmer;

        void Reset()
        {
            summonTimmer = 5000;
            me->CastSpell(me, SPELL_STORMLING_PRE_AURA, true);
        }

        void UpdateAI(uint32 diff) override
        {
            if (summonTimmer <= diff)
            {
                me->CastSpell(me, SPELL_STORMLING_SUMMON, true);
                me->DespawnOrUnsummon();
            }
            else summonTimmer -= diff;
        }
    };
};

// Stormling 47175
class npc_stormling : public CreatureScript
{
public:
    npc_stormling() : CreatureScript("npc_stormling") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_stormlingAI (creature);
    }

    struct npc_stormlingAI : public ScriptedAI
    {
        npc_stormlingAI(Creature* creature) : ScriptedAI(creature)
        {
            instance = me->GetInstanceScript();
        }

        InstanceScript* instance;

        void Reset()
        {
            me->CastSpell(me, SPELL_STORMLING_AURA, true);
            me->CastSpell(me, SPELL_STORMLING_AURA_VISUAL, true);
        }

        void JustDied(Unit* /*who*/)
        {
            if (Creature *boss = ObjectAccessor::GetCreature(*me, instance->GetData64(DATA_ALAKIR)))
            {
                boss->AI()->SetData(0, DATA_ALAKIR_BUFF);
            }
        }

        void IsSummonedBy(Unit* /*summoner*/)
        {
            if (Unit *target = SelectTarget(SELECT_TARGET_RANDOM, 0, 100.0f, true))
            {
                me->AI()->AttackStart(target);
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (!me->GetVictim())
                return;

            DoMeleeAttackIfReady();
        }
    };
};

// Electrocute 88427
class spell_electrocute: public SpellScriptLoader
{
public:
    spell_electrocute() : SpellScriptLoader("spell_electrocute") { }

    AuraScript* GetAuraScript() const
    {
        return new spell_electrocute_AuraScript();
    }

    class spell_electrocute_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_electrocute_AuraScript)

            void OnUpdate(AuraEffect* aurEff)
        {
            uint32 dmg = urand(103950, 106050);
            aurEff->SetAmount(dmg / (11 - (aurEff->GetTickNumber() / 2)));
        }

        void Register()
        {
            OnEffectUpdatePeriodic += AuraEffectUpdatePeriodicFn(spell_electrocute_AuraScript::OnUpdate, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
        }
    };
};

// Wind Burst 87770
class spell_wind_burst : public SpellScriptLoader
{
public:
    spell_wind_burst() : SpellScriptLoader("spell_wind_burst") { }

    SpellScript* GetSpellScript() const
    {
        return new spell_wind_burst_SpellScript();
    }

    class spell_wind_burst_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_wind_burst_SpellScript);


        void SummonTriggerOnHit(SpellEffIndex /*effIndex*/)
        {
            Unit* player = GetHitPlayer();
            if (!player)
                return;

            player->SummonCreature(NPC_WIND_BURST_TRIGGER, player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), 0.0f, TEMPSUMMON_MANUAL_DESPAWN);
        }

        void Register()
        {
            OnEffectHitTarget += SpellEffectFn(spell_wind_burst_SpellScript::SummonTriggerOnHit, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
        }
    };
};

// Wind Burst Trigger 8777000
class npc_wind_burst_trigger : public CreatureScript
{
public:
    npc_wind_burst_trigger() : CreatureScript("npc_wind_burst_trigger") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_wind_burst_triggerAI (creature);
    }

    struct npc_wind_burst_triggerAI : public ScriptedAI
    {
        npc_wind_burst_triggerAI(Creature* creature) : ScriptedAI(creature)
        {
            instance = me->GetInstanceScript();
            me->SetReactState(REACT_PASSIVE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
        }

        InstanceScript *instance;
        uint64 playerGUID;
        uint32 playerReturnTimer;

        void Reset()
        {
            playerGUID = 0;
            playerReturnTimer = 13000;
        }

        void IsSummonedBy(Unit* summoner)
        {
            if (summoner->GetTypeId() == TYPEID_PLAYER)
                playerGUID = summoner->GetGUID();
        }

        void UpdateAI(uint32 diff) override
        {
            if (playerReturnTimer <= diff)
            {
                if (Player* player = ObjectAccessor::GetPlayer(*me, playerGUID))
                {
                    if (player->HasAura(SPELL_RELENTLESS_STORM_VISUAL))
                    {
                        player->RemoveAurasDueToSpell(SPELL_RELENTLESS_STORM_VISUAL);
                        player->GetMotionMaster()->Clear();
                        player->GetMotionMaster()->MoveJump(me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), 20.0f, 6.0f);
                    }
                }
                me->DespawnOrUnsummon();
            }
            else playerReturnTimer -= diff;
        }
    };
};

// Relentless Storm Initial Vehicle 47806
class npc_relentless_storm_initial_vehicle : public CreatureScript
{
public:
    npc_relentless_storm_initial_vehicle() : CreatureScript("npc_relentless_storm_initial_vehicle") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_relentless_storm_initial_vehicleAI (creature);
    }

    struct npc_relentless_storm_initial_vehicleAI : public ScriptedAI
    {
        npc_relentless_storm_initial_vehicleAI(Creature* creature) : ScriptedAI(creature) 
        {
            instance = me->GetInstanceScript();
            me->SetReactState(REACT_PASSIVE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
        }

        InstanceScript* instance;

        void MovementInform(uint32 type, uint32 id)
        {
            if (type != POINT_MOTION_TYPE)
                return;

            if (id == 1)
            {
                me->DespawnOrUnsummon();
            }
        }

        void Reset()
        {
            me->SetSpeed(MOVE_WALK, 7.0f, true);
            me->SetSpeed(MOVE_RUN, 7.0f, true);
            me->SetSpeed(MOVE_FLIGHT, 7.0f, true);

            if (Creature *boss = ObjectAccessor::GetCreature(*me, instance->GetData64(DATA_ALAKIR)))
                me->GetMotionMaster()->MovePoint(1, me->GetPositionX() + (me->GetPositionX() - boss->GetPositionX()), me->GetPositionY() + (me->GetPositionY() - boss->GetPositionY()), me->GetPositionZ() + 80.0f);
        }
    };
};

// Lightning Coulds 48190
class npc_lightning_coulds : public CreatureScript
{
public:
    npc_lightning_coulds() : CreatureScript("npc_lightning_coulds") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_lightning_couldsAI (creature);
    }

    struct npc_lightning_couldsAI : public ScriptedAI
    {
        npc_lightning_couldsAI(Creature* creature) : ScriptedAI(creature) 
        {
            instance = me->GetInstanceScript();
            me->SetReactState(REACT_PASSIVE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_GRIP, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_INTERRUPT, false);
        }

        InstanceScript* instance;
        uint32 lightningVisual;
        uint32 readyDealDamageTimer;
        bool readyDealDamage;

        void Reset()
        {
            me->SetSpeed(MOVE_WALK, 0.2f, true);
            me->SetSpeed(MOVE_RUN, 0.2f, true);
            lightningVisual = 3000;
            readyDealDamage = false;
            readyDealDamageTimer = 4000;
        }

        void SetData(uint32 uiI, uint32 uiValue)
        {
            if (uiValue == DATA_CLOUDS_DEAL_DMG)
            {
                readyDealDamage = true;
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (lightningVisual <= diff)
            {
                me->CastSpell(me, SPELL_LIGHTNING_CLOUDS_SINGLE_VIS, true);
                lightningVisual = urand(1000, 6000);
            }
            else lightningVisual -= diff;

            if (readyDealDamage)
            {
                if (readyDealDamageTimer <= diff)
                {
                    readyDealDamage = false;
                    me->CastSpell(me, SPELL_LIGHTNING_CLOUDS_DMG, true);
                }
                else readyDealDamageTimer -= diff;
            }
        }
    };
};

// Relentless Storm 47807
class npc_relentless_storm : public CreatureScript
{
public:
    npc_relentless_storm() : CreatureScript("npc_relentless_storm") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_relentless_stormAI (creature);
    }

    struct npc_relentless_stormAI : public ScriptedAI
    {
        npc_relentless_stormAI(Creature* creature) : ScriptedAI(creature) 
        {
            instance = me->GetInstanceScript();
            me->SetReactState(REACT_PASSIVE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
        }

        InstanceScript* instance;

        float angelStorm;
        float radiusStorm;
        uint64 triggerGUID;
        uint32 changeAngelTimer;

        void Reset()
        {
            me->SetSpeed(MOVE_WALK, 5.0f, true);
            me->SetSpeed(MOVE_RUN, 5.0f, true);
            me->SetSpeed(MOVE_FLIGHT, 5.0f, true);

            changeAngelTimer = 1;
            radiusStorm = me->GetDistance2d(CenterPoint.GetPositionX(), CenterPoint.GetPositionY());
            if (urand(0, 1))
                angelStorm = 1.0f;
            else
                angelStorm = -1.0f;
            if (Creature* trigger = me->SummonCreature(NPC_ICE_STORM_ROTATE_TRIGGER, CenterPoint, TEMPSUMMON_TIMED_DESPAWN, 10000))
                triggerGUID = trigger->GetGUID();
        }

        void RunArc()
        {
            if (Creature* trigger = ObjectAccessor::GetCreature(*me, triggerGUID))
            {
                trigger->SetOrientation(trigger->GetAngle(me));
                float orient = trigger->GetOrientation();
                trigger->SetOrientation(orient + angelStorm);

                float x, y, z;
                trigger->GetClosePoint(x, y, z, me->GetObjectSize() / 3, radiusStorm);

                me->GetMotionMaster()->Clear(true);
                me->GetMotionMaster()->MovePoint(0, x, y, me->GetPositionZ());
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (changeAngelTimer <= diff)
            {
                RunArc();
                changeAngelTimer = 1000;
            }
            else changeAngelTimer -= diff;
        }
    };
};

class PlayerRangeCheck
{
public:
    explicit PlayerRangeCheck(Unit* _caster) : caster(_caster) { }

    bool operator() (WorldObject* unit)
    {
        if (unit->GetTypeId() != TYPEID_PLAYER || (caster->GetPositionZ() + 10.0f > unit->GetPositionZ() && caster->GetPositionZ() - 10.0f < unit->GetPositionZ()))
            return false;

        return true;
    }

    Unit* caster;
};

class spell_lightning_clouds_damage : public SpellScriptLoader
{
public:
    spell_lightning_clouds_damage() : SpellScriptLoader("spell_lightning_clouds_damage") { }

    SpellScript* GetSpellScript() const
    {
        return new spell_lightning_clouds_damage_SpellScript();
    }

    class spell_lightning_clouds_damage_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_lightning_clouds_damage_SpellScript);

        void FilterTargets(std::list<WorldObject*>& unitList)
        {
            unitList.remove_if(PlayerRangeCheck(GetCaster()));
        }

        void Register()
        {
            OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_lightning_clouds_damage_SpellScript::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
        }
    };
};

class spell_lightning_strike : public SpellScriptLoader
{
public:
    spell_lightning_strike() : SpellScriptLoader("spell_lightning_strike") { }

    SpellScript* GetSpellScript() const
    {
        return new spell_lightning_strike_SpellScript();
    }

    class spell_lightning_strike_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_lightning_strike_SpellScript);

        void HandleDummyHitTarget(SpellEffIndex /*effIndex*/)
        {
            Unit* target = GetHitUnit();
            if (target && target->GetTypeId() == TYPEID_PLAYER)
            {
                target->CastSpell(target, SPELL_LIGHTNING_STRIKE_EFFECT, true);
            }
        }

        void Register()
        {
            OnEffectHitTarget += SpellEffectFn(spell_lightning_strike_SpellScript::HandleDummyHitTarget, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
        }
    };
};

class spell_lightning_strike_effect : public SpellScriptLoader
{
public:
    spell_lightning_strike_effect() : SpellScriptLoader("spell_lightning_strike_effect") { }

    SpellScript* GetSpellScript() const
    {
        return new spell_lightning_strike_effect_SpellScript();
    }

    class spell_lightning_strike_effect_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_lightning_strike_effect_SpellScript);

        void HandleAfterCast()
        {
            std::list<Player*> players;
            Trinity::AnyPlayerInObjectRangeCheck checker(GetCaster(), 20.0f);
            Trinity::PlayerListSearcher<Trinity::AnyPlayerInObjectRangeCheck> searcher(GetCaster(), players, checker);
            GetCaster()->VisitNearbyWorldObject(20.0f, searcher);

            for (std::list<Player*>::const_iterator itr = players.begin(); itr != players.end(); ++itr)
            {
                if (GetCaster()->ToPlayer()->GetGUID() != (*itr)->GetGUID())
                    GetCaster()->CastSpell((*itr), SPELL_LIGHTNING_STRIKE_SINGLE, true);
            }
        }

        void Register()
        {
            AfterCast += SpellCastFn(spell_lightning_strike_effect_SpellScript::HandleAfterCast);
        }
    };
};

class AreaTrigger_at_reletness_storm : public AreaTriggerScript
{
public:
    AreaTrigger_at_reletness_storm() : AreaTriggerScript("at_reletness_storm") { }

    bool OnTrigger(Player* player, AreaTriggerEntry const* /*trigger*/)
    {
        InstanceScript* instance = player->GetInstanceScript();
        if (instance)
        {
            if (player->IsAlive())
            {
                if (instance->GetData(DATA_ALAKIR_FLIGHT_PHASE) == IN_PROGRESS)
                {       
                    if (Creature* trigger = player->SummonCreature(NPC_ICE_STORM_ROTATE_TRIGGER, CenterPoint, TEMPSUMMON_TIMED_DESPAWN, 5000))
                    {
                        trigger->SetOrientation(trigger->GetAngle(player));
                        float x, y, z;
                        trigger->GetClosePoint(x, y, z, trigger->GetObjectSize() / 3, 80.0f);

                        if (Creature* storm = player->SummonCreature(NPC_RELENTLESS_STORM, x, y, player->GetPositionZ(), 0.0f, TEMPSUMMON_MANUAL_DESPAWN))
                        {
                            player->CastSpell(player, SPELL_RELENTLESS_STORM_VEHICLE, true);
                            player->EnterVehicle(storm);
                            storm->DespawnOrUnsummon(6000);
                        }
                    }
                }
            }
        }
        return true;
    }
};

void AddSC_boss_alakir()
{
    new boss_alakir();
    new npc_squall_line();
    new npc_wind_funnel();
    new npc_ice_storm();
    new npc_ice_storm_rain();
    new npc_stormling_pre_effect();
    new npc_stormling();
    new spell_electrocute();
    new spell_wind_burst();
    // new npc_wind_burst_trigger();
    new npc_relentless_storm_initial_vehicle();
    new npc_lightning_coulds();
    new npc_relentless_storm();
    new spell_lightning_clouds_damage();
    new spell_lightning_strike();
    new spell_lightning_strike_effect();
    new AreaTrigger_at_reletness_storm();
}
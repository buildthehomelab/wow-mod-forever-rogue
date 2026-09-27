/*
 * mod-forever-rogue
 *
 * Rogue combo points follow the rogue from target to target, as in WoW Forever. In stock 3.3.5,
 * combo points belong to one target: attacking another target starts over at zero, and killing
 * the target loses them. With this module, unused combo points move to whatever hostile target
 * the rogue selects or attacks next, with the full count. Points left on a target that died wait
 * a little while (ForeverRogue.ComboPoints.KeepAfterKill) for the next target.
 *
 * No client patch: the server tells the client which target the points are on, so the target
 * frame shows them on the new target right away.
 *
 * Released under the MIT License.
 */

#include "Config.h"
#include "DataMap.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellInfo.h"

namespace
{
    struct Config
    {
        bool enabled = true;
        uint32 keepAfterKill = 20000;
    };

    Config config;

    // After a finisher, points that vanish belong to the finisher, not to a dying target. Deadly
    // Throw flies to its target, so its points only go when it lands.
    constexpr uint32 FINISHER_WINDOW = 3000;

    struct ComboState : public DataMap::Base
    {
        // Points and target as of the last update, to tell what was lost when the target died.
        uint8 lastPoints = 0;
        ObjectGuid lastTarget;

        // Points from a target that died, waiting for the next target.
        uint8 savedPoints = 0;
        uint32 savedTimer = 0;

        // Time left in which a finisher may still take the points.
        uint32 finisherTimer = 0;

        // Points held when a builder was used, in case the builder kills its target: the core
        // clears the points at the death and then adds the builder's points to the corpse from
        // zero.
        bool builderPending = false;
        uint8 builderBase = 0;
        ObjectGuid builderTarget;
    };

    ComboState* GetState(Player* player)
    {
        return player->CustomData.GetDefault<ComboState>("mod-forever-rogue");
    }

    bool IsRogue(Player* player)
    {
        return config.enabled && player->getClass() == CLASS_ROGUE;
    }

    bool CanTakePoints(Player* player, Unit* target)
    {
        return target && target != player && target->IsAlive() && player->IsValidAttackTarget(target);
    }

    // Moves the rogue's combo points, or points saved from a dead target, onto the new target.
    void MovePointsTo(Player* player, ComboState* state, Unit* target)
    {
        if (!CanTakePoints(player, target) || player->GetComboTarget() == target)
            return;

        uint8 points = player->GetComboPoints();
        if (!points)
        {
            points = state->savedPoints;
            if (!points)
                return;
        }

        state->savedPoints = 0;
        state->savedTimer = 0;

        // For a new target, AddComboPoints sets the count instead of adding to it. It also moves
        // the points' holder and tells the client, which then shows them on the new target.
        player->AddComboPoints(target, points);
    }
}

class ForeverRogueWorldScript : public WorldScript
{
public:
    ForeverRogueWorldScript() : WorldScript("ForeverRogueWorldScript") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        config.enabled       = sConfigMgr->GetOption<bool>("ForeverRogue.ComboPoints.Enable", true);
        config.keepAfterKill = sConfigMgr->GetOption<uint32>("ForeverRogue.ComboPoints.KeepAfterKill", 20000);
    }
};

class ForeverRoguePlayerScript : public PlayerScript
{
public:
    ForeverRoguePlayerScript() : PlayerScript("ForeverRoguePlayerScript", { PLAYERHOOK_ON_UPDATE, PLAYERHOOK_ON_SPELL_CAST }) { }

    // Runs before the spell's effects and before its last cast check, so a builder or finisher
    // used on a target that isn't selected (a mouseover or focus macro) finds the points there.
    void OnPlayerSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (!IsRogue(player))
            return;

        SpellInfo const* spellInfo = spell->GetSpellInfo();
        ComboState* state = GetState(player);

        if (spellInfo->NeedsComboPoints())
        {
            state->lastPoints = 0;
            state->savedPoints = 0;
            state->savedTimer = 0;
            state->builderPending = false;
            state->finisherTimer = FINISHER_WINDOW;
            return;
        }

        if (!spellInfo->HasEffect(SPELL_EFFECT_ADD_COMBO_POINTS))
            return;

        Unit* target = spell->m_targets.GetUnitTarget();
        if (!target)
            return;

        MovePointsTo(player, state, target);

        // A proc like Seal Fate casts a second builder at the same target before the first one
        // is done; keep the count from before the first.
        if (state->builderPending && state->builderTarget == target->GetGUID())
            return;

        state->builderPending = true;
        state->builderBase = player->GetComboTarget() == target ? player->GetComboPoints() : 0;
        state->builderTarget = target->GetGUID();
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (!IsRogue(player))
            return;

        ComboState* state = GetState(player);

        if (!player->IsAlive())
        {
            *state = ComboState();
            return;
        }

        state->finisherTimer = state->finisherTimer > diff ? state->finisherTimer - diff : 0;

        if (state->savedTimer)
        {
            state->savedTimer = state->savedTimer > diff ? state->savedTimer - diff : 0;
            if (!state->savedTimer)
                state->savedPoints = 0;
        }

        // A builder killed its target: put back the points the death took.
        if (state->builderPending)
        {
            state->builderPending = false;

            Unit* target = ObjectAccessor::GetUnit(*player, state->builderTarget);
            if (config.keepAfterKill && state->builderBase && player->GetComboPoints() && player->GetComboTargetGUID() == state->builderTarget
                && (!target || !target->IsAlive()))
                player->AddComboPoints(state->builderBase);
        }

        uint8 const points = player->GetComboPoints();

        if (points)
        {
            state->lastPoints = points;
            state->lastTarget = player->GetComboTargetGUID();
        }
        else if (state->lastPoints)
        {
            // The points are gone. Keep them only if their target died or despawned; a finisher,
            // the end of a duel or Premeditation running out leave the target alive.
            Unit* target = ObjectAccessor::GetUnit(*player, state->lastTarget);
            if ((!target || !target->IsAlive()) && !state->finisherTimer && config.keepAfterKill)
            {
                state->savedPoints = state->lastPoints;
                state->savedTimer = config.keepAfterKill;
            }

            state->lastPoints = 0;
            state->lastTarget.Clear();
        }

        MovePointsTo(player, state, player->GetSelectedUnit());
    }
};

void AddForeverRogueScripts()
{
    new ForeverRogueWorldScript();
    new ForeverRoguePlayerScript();
}

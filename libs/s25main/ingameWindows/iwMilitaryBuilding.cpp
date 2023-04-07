//
// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "iwMilitaryBuilding.h"
#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "Loader.h"
#include "WindowManager.h"
#include "addons/const_addons.h"
#include "buildings/nobMilitary.h"
#include "controls/ctrlImageButton.h"
#include "controls/ctrlProgress.h"
#include "figures/nofPassiveSoldier.h"
#include "helpers/containerUtils.h"
#include "helpers/toString.h"
#include "iwDemolishBuilding.h"
#include "iwHelp.h"
#include "iwMsgbox.h"
#include "network/GameClient.h"
#include "ogl/FontStyle.h"
#include "ogl/glArchivItem_Bitmap.h"
#include "ogl/glFont.h"
#include "world/GameWorldBase.h"
#include "world/GameWorldView.h"
#include "gameData/BuildingConsts.h"
#include "gameData/MilitaryConsts.h"
#include "gameData/const_gui_ids.h"
#include "gameData/SettingTypeConv.h"
#include <set>
#include <controls/ctrlTextDeepening.h>

iwMilitaryBuilding::iwMilitaryBuilding(GameWorldView& gwv, GameCommandFactory& gcFactory, nobMilitary* const building)
    : IngameWindow(CGI_BUILDING + MapBase::CreateGUIID(building->GetPos()), IngameWindow::posAtMouse, Extent(246, 214),
                   _(BUILDING_NAMES[building->GetBuildingType()]), LOADER.GetImageN("resource", 41)),
      gwv(gwv), gcFactory(gcFactory), building(building)
{
    const int bottomRowY = 167;

    // Schwert
    AddImage(0, DrawPoint(28, 39), LOADER.GetMapTexture(2298));
    AddImage(1, DrawPoint(28, 39), LOADER.GetWareTex(GoodType::Sword));

    // Schild
    AddImage(2, DrawPoint(216, 39), LOADER.GetMapTexture(2298));
    AddImage(3, DrawPoint(216, 39), LOADER.GetWareTex(GoodType::ShieldRomans));

    // Hilfe
    AddImageButton(4, DrawPoint(16, bottomRowY), Extent(30, 32), TextureColor::Grey, LOADER.GetImageN("io", 225),
                   _("Help"));
    // Abreißen
    AddImageButton(5, DrawPoint(50, bottomRowY), Extent(34, 32), TextureColor::Grey, LOADER.GetImageN("io", 23),
                   _("Demolish house"));
    // Gold an/aus (227,226)
    AddImageButton(6, DrawPoint(90, bottomRowY), Extent(32, 32), TextureColor::Grey,
                   LOADER.GetImageN("io", ((building->IsGoldDisabledVirtual()) ? 226 : 227)), _("Gold delivery"));
    // "Gehe Zu Ort"
    AddImageButton(7, DrawPoint(199, bottomRowY), Extent(30, 32), TextureColor::Grey, LOADER.GetImageN("io", 107),
                   _("Go to place"));

    // Gebäudebild
    AddImage(8, DrawPoint(127, 124), &building->GetBuildingImage());
    // "Go to next" (building of same type)
    AddImageButton(9, DrawPoint(199, 135), Extent(30, 32), TextureColor::Grey, LOADER.GetImageN("io_new", 11),
                   _("Go to next military building"));
    // addon military control active? -> show button
    if(gwv.GetWorld().GetGGS().isEnabled(AddonId::MILITARY_CONTROL))
        AddImageButton(10, DrawPoint(124, bottomRowY), Extent(30, 32), TextureColor::Grey,
                       LOADER.GetImageN("io_new", 12),
                       _("Send max rank soldiers to a warehouse"));

    if(gwv.GetWorld().GetGGS().isEnabled(AddonId::MILITARY_CONTROL))
        AddImageButton(11, DrawPoint(156, bottomRowY), Extent(30, 32), TextureColor::Grey,
                       LOADER.GetImageN("io_new", 15),
                       _("Send min rank soldiers to a warehouse"));

    AddImageButton(12, DrawPoint(45, 25), Extent(32, 32), TextureColor::Grey,
                   LOADER.GetImageN("io", ((building->IsMilitaryOverrideEnabledVirtual()) ? 191 : 133)),
                   _("Override military settings"));

    AddProgress(13, DrawPoint(16, 137), Extent(180, 26), TextureColor::Grey, 123, 124, MILITARY_SETTINGS_SCALE[7],
        "", Extent(4, 4), 0, _("Fewer soldeirs"), _("More soldiers"));

    AddTextDeepening(14, DrawPoint(79, 25), Extent(32, 32), TextureColor::Grey, "-", NormalFont,  COLOR_YELLOW);
}

void iwMilitaryBuilding::Draw_()
{
    IngameWindow::Draw_();

    // Schwarzer Untergrund für Goldanzeige
    const unsigned maxCoinCt = building->GetMaxCoinCt();
    DrawPoint goldPos = GetDrawPos() + DrawPoint((GetSize().x - 22 * maxCoinCt) / 2, 60);
    DrawRectangle(Rect(goldPos, Extent(22 * maxCoinCt, 24)), 0x96000000);
    // Gold
    goldPos += DrawPoint(12, 12);
    for(unsigned short i = 0; i < maxCoinCt; ++i)
    {
        LOADER.GetMapTexture(2278)->DrawFull(goldPos, (i >= building->GetNumCoins() ? 0xFFA0A0A0 : 0xFFFFFFFF));
        goldPos.x += 22;
    }

    // Sammeln aus der Rausgeh-Liste und denen, die wirklich noch drinne sind
    boost::container::flat_set<const nofSoldier*, ComparatorSoldiersByRank> soldiers;
    for(const auto& soldier : building->GetTroops())
        soldiers.insert(&soldier);
    for(const noFigure& fig : building->GetLeavingFigures())
    {
        const GO_Type figType = fig.GetGOT();
        if(figType == GO_Type::NofAttacker || figType == GO_Type::NofAggressivedefender
           || figType == GO_Type::NofDefender || figType == GO_Type::NofPassivesoldier)
        {
            soldiers.insert(static_cast<const nofSoldier*>(&fig));
        }
    }

    const unsigned maxSoldierCt = building->GetMaxTroopsCt();
    DrawPoint troopsPos = GetDrawPos() + DrawPoint((GetSize().x - 22 * maxSoldierCt) / 2, 98);
    // Schwarzer Untergrund für Soldatenanzeige
    DrawRectangle(Rect(troopsPos, Extent(22 * maxSoldierCt, 24)), 0x96000000);

    // Soldaten zeichnen
    DrawPoint curTroopsPos = troopsPos + DrawPoint(12, 12);
    for(const auto* soldier : soldiers)
    {
        LOADER.GetMapTexture(2321 + soldier->GetRank())->DrawFull(curTroopsPos);
        curTroopsPos.x += 22;
    }

    // Draw health above soldiers
    if(gwv.GetWorld().GetGGS().isEnabled(AddonId::MILITARY_HITPOINTS))
    {
        DrawPoint healthPos = troopsPos - DrawPoint(0, 14);

        // black background for hitpoints
        DrawRectangle(Rect(healthPos, Extent(22 * maxSoldierCt, 14)), 0x96000000);

        healthPos += DrawPoint(12, 2);
        for(const auto* soldier : soldiers)
        {
            auto hitpoints = static_cast<int>(soldier->GetHitpoints());
            auto maxHitpoints = static_cast<int>(HITPOINTS[soldier->GetRank()]);
            unsigned hitpointsColour;
            if(hitpoints <= maxHitpoints / 2)
                hitpointsColour = COLOR_RED;
            else
            {
                if(hitpoints == maxHitpoints)
                    hitpointsColour = COLOR_GREEN;
                else
                    hitpointsColour = COLOR_ORANGE;
            }
            NormalFont->Draw(healthPos, std::to_string(hitpoints), FontStyle::CENTER, hitpointsColour);
            healthPos.x += 22;
        }
    }

    GetCtrl<ctrlTextDeepening>(14)->SetText(helpers::toString(building->CalcRequiredNumTroops()));

    GetCtrl<ctrlProgress>(13)->SetPosition(building->GetCurrentMilitarySetting());
}

void iwMilitaryBuilding::Msg_ButtonClick(const unsigned ctrl_id)
{
    switch(ctrl_id)
    {
        case 4: // Hilfe
        {
            WINDOWMANAGER.ReplaceWindow(
              std::make_unique<iwHelp>(_(BUILDING_HELP_STRINGS[building->GetBuildingType()])));
        }
        break;
        case 5: // Gebäude abbrennen
        {
            // Darf das Gebäude abgerissen werden?
            if(!building->IsDemolitionAllowed())
            {
                // Messagebox anzeigen
                DemolitionNotAllowed(gwv.GetWorld().GetGGS());
            } else
            {
                // Abreißen?
                Close();
                WINDOWMANAGER.Show(std::make_unique<iwDemolishBuilding>(gwv, building));
            }
        }
        break;
        case 6: // Gold einstellen/erlauben
        {
            if(!GAMECLIENT.IsReplayModeOn())
            {
                // NC senden
                if(gcFactory.SetCoinsAllowed(building->GetPos(), building->IsGoldDisabledVirtual()))
                {
                    // visuell anzeigen
                    building->ToggleCoinsVirtual();
                    // anderes Bild auf dem Button
                    if(building->IsGoldDisabledVirtual())
                        GetCtrl<ctrlImageButton>(6)->SetImage(LOADER.GetImageN("io", 226));
                    else
                        GetCtrl<ctrlImageButton>(6)->SetImage(LOADER.GetImageN("io", 227));
                }
            }
        }
        break;
        case 7: // "Gehe Zu Ort"
        {
            gwv.MoveToMapPt(building->GetPos());
        }
        break;
        case 9: // go to next of same type
        {
            const std::list<nobMilitary*>& militaryBuildings =
              gwv.GetWorld().GetPlayer(building->GetPlayer()).GetBuildingRegister().GetMilitaryBuildings();
            // go through list once we get to current building -> open window for the next one and go to next location
            auto it = helpers::find_if(
              militaryBuildings, [bldPos = building->GetPos()](const auto* it) { return it->GetPos() == bldPos; });
            if(it != militaryBuildings.end()) // got to current building in the list?
            {
                // close old window, open new window (todo: only open if it isnt already open), move to location of next
                // building
                Close();
                ++it;
                if(it == militaryBuildings.end()) // was last entry in list -> goto first
                    it = militaryBuildings.begin();
                gwv.MoveToMapPt((*it)->GetPos());
                WINDOWMANAGER.ReplaceWindow(std::make_unique<iwMilitaryBuilding>(gwv, gcFactory, *it)).SetPos(GetPos());
                break;
            }
        }
        break;
        case 10: // send home button (addon)
        {
            gcFactory.SendSoldiersHome(building->GetPos());
        }
        break;
        case 11: // send min home button (addon)
        {
            gcFactory.SendWorstSoldiersHome(building->GetPos());
        }
        break;
        case 12: 
        {
            if(!GAMECLIENT.IsReplayModeOn())
            {
                // visuell anzeigen
                building->ToggleMilitaryEnabledVirtual();

                // NC senden
                if(gcFactory.SetMilitaryOverrideAllowed(building->GetPos(),
                                                        building->IsMilitaryOverrideEnabledVirtual(),
                                                        (unsigned char)GetCtrl<ctrlProgress>(13)->GetPosition()))
                {
                    // anderes Bild auf dem Button
                    if(building->IsMilitaryOverrideEnabledVirtual())
                        GetCtrl<ctrlImageButton>(12)->SetImage(LOADER.GetImageN("io", 191));
                    else
                        GetCtrl<ctrlImageButton>(12)->SetImage(LOADER.GetImageN("io", 133));
                }
            }
        }
        break;
    }
}

void iwMilitaryBuilding::Msg_ProgressChange(unsigned ctrl_id, unsigned short position) 
{
    gcFactory.SetMilitaryOverrideAllowed(building->GetPos(), building->IsMilitaryOverrideEnabledVirtual(),
                                         (unsigned char)GetCtrl<ctrlProgress>(13)->GetPosition());
}

void iwMilitaryBuilding::DemolitionNotAllowed(const GlobalGameSettings& ggs)
{
    // Meldung auswählen, je nach Einstellung
    std::string msg;
    switch(ggs.getSelection(AddonId::DEMOLITION_PROHIBITION))
    {
        default: RTTR_Assert(false); break;
        case 1: msg = _("Demolition ist not allowed because the building is under attack!"); break;
        case 2: msg = _("Demolition ist not allowed because the building is located in border area!"); break;
    }

    WINDOWMANAGER.Show(std::make_unique<iwMsgbox>(_("Demolition not possible"), msg, nullptr, MsgboxButton::Ok,
                                                  MsgboxIcon::ExclamationRed));
}

//====== Copyright Custom Fortress 2, All rights reserved. =================
//
// Workshop Weapon Mod Creation Tool Implementation
//
//=============================================================================

#include "cbase.h"
#include "../cf/cf_workshop_weapon_tool.h"
#include "../cf/cf_workshop_manager.h"
#include <vgui/IVGui.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/IInput.h>
#include <vgui_controls/FileOpenDialog.h>
#include <vgui_controls/DirectorySelectDialog.h>
#include <vgui_controls/MessageBox.h>
#include "vgui/ILocalize.h"
#include "ienginevgui.h"
#include "filesystem.h"
#include "tier1/KeyValues.h"
#include "econ_item_system.h"
#include "tf_item_schema.h"
#include "tf_shareddefs.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "materialsystem/imaterialsystem.h"
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif
#include <stdio.h>

#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Helper functions for weapon lookups from item schema
//-----------------------------------------------------------------------------
namespace vgui
{

// Check if a loadout slot is a weapon slot
static bool IsWeaponLoadoutSlot(int iSlot)
{
	return iSlot == LOADOUT_POSITION_PRIMARY ||
		   iSlot == LOADOUT_POSITION_SECONDARY ||
		   iSlot == LOADOUT_POSITION_MELEE ||
		   iSlot == LOADOUT_POSITION_BUILDING ||
		   iSlot == LOADOUT_POSITION_PDA ||
		   iSlot == LOADOUT_POSITION_PDA2;
}

// Get display name for an item definition (localized if available)
static const char* GetItemDisplayName(const CTFItemDefinition* pItemDef)
{
	if (!pItemDef)
		return "";
	
	const char* pszBaseName = pItemDef->GetItemBaseName();
	if (pszBaseName && pszBaseName[0] == '#')
	{
		// Try to get localized name
		const wchar_t* pwszLocalized = g_pVGuiLocalize->Find(pszBaseName);
		if (pwszLocalized)
		{
			// Convert to UTF-8 for display
			static char szLocalizedName[256];
			g_pVGuiLocalize->ConvertUnicodeToANSI(pwszLocalized, szLocalizedName, sizeof(szLocalizedName));
			return szLocalizedName;
		}
	}
	
	// Fall back to definition name
	return pItemDef->GetDefinitionName();
}

// Get the best model path for a weapon (prefer c_model for view/world unified models)
static const char* GetWeaponModelPath(const CTFItemDefinition* pItemDef)
{
	if (!pItemDef)
		return "";
	
	// Try to get the base player display model first (c_model)
	const char* pszModel = pItemDef->GetBasePlayerDisplayModel();
	if (pszModel && pszModel[0])
		return pszModel;
	
	// Try world display model
	pszModel = pItemDef->GetWorldDisplayModel();
	if (pszModel && pszModel[0])
		return pszModel;
	
	return "";
}

} // namespace vgui

using namespace vgui;

//-----------------------------------------------------------------------------
// CWeaponModelPreviewPanel
//-----------------------------------------------------------------------------
CWeaponModelPreviewPanel::CWeaponModelPreviewPanel(Panel* parent, const char* panelName)
	: BaseClass(parent, panelName)
	, m_bAutoRotate(false)
	, m_flAutoRotateSpeed(30.0f)
{
	SetLookAtCamera(true);
	
	// Enable full manipulation which includes:
	// Left-click: rotation
	// Middle-click: translate
	// Right-click: zoom (drag up/down)
	// Scroll wheel: zoom
	m_bAllowRotation = true;
	m_bAllowPitch = true;
	m_bAllowFullManipulation = true;
	SetMouseInputEnabled(true);
}

CWeaponModelPreviewPanel::~CWeaponModelPreviewPanel() {}

void CWeaponModelPreviewPanel::SetMDL(MDLHandle_t handle, void *pProxyData)
{
	// Guard against invalid handles - base CMDLPanel crashes without this check
	if (handle == MDLHANDLE_INVALID)
	{
		m_RootMDL.m_MDL.SetMDL(MDLHANDLE_INVALID);
		if (m_RootMDL.m_pStudioHdr)
		{
			delete m_RootMDL.m_pStudioHdr;
			m_RootMDL.m_pStudioHdr = NULL;
		}
		return;
	}
	
	// Verify the studiohdr is valid before letting base class create CStudioHdr
	studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr(handle);
	if (!pStudioHdr)
	{
		Warning("CWeaponModelPreviewPanel::SetMDL - Invalid studio header for handle\n");
		m_RootMDL.m_MDL.SetMDL(MDLHANDLE_INVALID);
		return;
	}
	
	// Safe to call base class now
	BaseClass::SetMDL(handle, pProxyData);
}

void CWeaponModelPreviewPanel::SetMDL(const char *pMDLName, void *pProxyData)
{
	if (!pMDLName || !pMDLName[0])
	{
		SetMDL(MDLHANDLE_INVALID, pProxyData);
		return;
	}
	
	// Find the MDL handle
	MDLHandle_t hMDL = g_pMDLCache->FindMDL(pMDLName);
	if (hMDL == MDLHANDLE_INVALID)
	{
		Warning("CWeaponModelPreviewPanel::SetMDL - FindMDL failed for: %s\n", pMDLName);
		return;
	}
	
	// Check for error model
	if (g_pMDLCache->IsErrorModel(hMDL))
	{
		Warning("CWeaponModelPreviewPanel::SetMDL - Error model for: %s\n", pMDLName);
		g_pMDLCache->Release(hMDL);
		return;
	}
	
	// Manually do what CMDLPanel::SetMDL(handle) does, but safely:
	// 1. Set the MDL handle (this adds a reference)
	m_RootMDL.m_MDL.SetMDL(hMDL);
	
	// 2. Create the CStudioHdr safely
	if (m_RootMDL.m_pStudioHdr)
	{
		delete m_RootMDL.m_pStudioHdr;
		m_RootMDL.m_pStudioHdr = NULL;
	}
	
	studiohdr_t *pStudioHdr = m_RootMDL.m_MDL.GetStudioHdr();
	if (pStudioHdr)
	{
		m_RootMDL.m_pStudioHdr = new CStudioHdr(pStudioHdr, g_pMDLCache);
	}
	
	m_RootMDL.m_MDL.m_pProxyData = pProxyData;
	
	// 3. Setup bounding box and view target
	Vector vecMins, vecMaxs;
	GetMDLBoundingBox(&vecMins, &vecMaxs, hMDL, m_RootMDL.m_MDL.m_nSequence);
	m_RootMDL.m_MDL.m_bWorldSpaceViewTarget = false;
	m_RootMDL.m_MDL.m_vecViewTarget.Init(100.0f, 0.0f, vecMaxs.z);
	m_RootMDL.m_flCycleStartTime = 0.f;
	
	// 4. Release our FindMDL reference (CMDL::SetMDL added its own)
	g_pMDLCache->Release(hMDL);
	
	// 5. Let CBaseModelPanel do its setup (sequence, flex controllers, etc.)
	SetSequence(ACT_IDLE);
	SetupModelDefaults();
	InvalidateLayout();
}

bool CWeaponModelPreviewPanel::LoadModel(const char* pszModelPath)
{
	if (!pszModelPath || !pszModelPath[0])
		return false;
	
	// Check if this is an absolute path - we need to convert it to a relative path
	char szRelativePath[MAX_PATH];
	
	// Try to use the path as-is first (relative path)
	if (g_pFullFileSystem->FileExists(pszModelPath, "GAME"))
	{
		Q_strncpy(szRelativePath, pszModelPath, sizeof(szRelativePath));
	}
	else
	{
		// Try to extract a relative path from an absolute path
		const char* pModelsDir = V_stristr(pszModelPath, "models\\");
		if (!pModelsDir)
			pModelsDir = V_stristr(pszModelPath, "models/");
		
		if (pModelsDir)
		{
			Q_strncpy(szRelativePath, pModelsDir, sizeof(szRelativePath));
			Q_FixSlashes(szRelativePath, '/');
		}
		else
		{
			Warning("CWeaponModelPreviewPanel::LoadModel - Could not find 'models' folder in path: %s\n", pszModelPath);
			return false;
		}
	}
	
	// Verify the model file exists
	if (!g_pFullFileSystem->FileExists(szRelativePath, "GAME"))
	{
		Warning("CWeaponModelPreviewPanel::LoadModel - Model file not found: %s\n", szRelativePath);
		return false;
	}
	
	Msg("CWeaponModelPreviewPanel::LoadModel - Loading: %s\n", szRelativePath);
	
	// Load the model
	SetMDL(szRelativePath);
	
	// Check if model was loaded successfully
	MDLHandle_t hMDL = m_RootMDL.m_MDL.GetMDL();
	Msg("CWeaponModelPreviewPanel::LoadModel - Handle: %d\n", (int)hMDL);
	
	if (hMDL != MDLHANDLE_INVALID)
	{
		ResetCamera();
		return true;
	}
	
	Warning("CWeaponModelPreviewPanel::LoadModel - Model handle is invalid after SetMDL\n");
	return false;
}

void CWeaponModelPreviewPanel::ClearModel()
{
	// Use empty string which base class handles
	SetMDL((const char*)NULL);
}

void CWeaponModelPreviewPanel::ResetCamera()
{
	// Reset all rotation and position
	m_angPlayer.Init();
	m_vecPlayerPos.Init();
	
	// Reset the model pose
	m_BMPResData.m_angModelPoseRot.Init();
	m_BMPResData.m_vecOriginOffset.Init();
	m_BMPResData.m_vecFramedOriginOffset.Init();
	m_BMPResData.m_vecViewportOffset.Init();
	
	// Re-center camera on model
	LookAtMDL();
}

void CWeaponModelPreviewPanel::SetAutoRotate(bool b) { m_bAutoRotate = b; }

void CWeaponModelPreviewPanel::OnThink()
{
	BaseClass::OnThink();
	
	// Auto-rotate when enabled and not being manipulated
	if (m_bAutoRotate && !m_bMousePressed)
	{
		RotateYaw(gpGlobals->frametime * m_flAutoRotateSpeed);
	}
}

void CWeaponModelPreviewPanel::ApplySchemeSettings(IScheme* pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetPaintBackgroundEnabled(true);
	SetPaintBackgroundType(0);
	SetBackgroundColor(Color(30, 28, 27, 255));
}

void CWeaponModelPreviewPanel::PaintBackground()
{
	int wide, tall;
	GetSize(wide, tall);
	
	// Draw solid dark background
	surface()->DrawSetColor(Color(30, 28, 27, 255));
	surface()->DrawFilledRect(0, 0, wide, tall);
}

void CWeaponModelPreviewPanel::OnMousePressed(MouseCode code)
{
	// Let base class handle all mouse input - it supports:
	// Left-click: rotation
	// Middle-click: translate
	// Right-click: zoom
	BaseClass::OnMousePressed(code);
}

void CWeaponModelPreviewPanel::OnMouseReleased(MouseCode code)
{
	BaseClass::OnMouseReleased(code);
}

void CWeaponModelPreviewPanel::OnCursorMoved(int x, int y)
{
	BaseClass::OnCursorMoved(x, y);
}

void CWeaponModelPreviewPanel::OnMouseWheeled(int delta)
{
	// Use base class zoom - requires m_bAllowFullManipulation to be true
	BaseClass::OnMouseWheeled(delta);
}

//-----------------------------------------------------------------------------
// CCFWorkshopWeaponTool
//-----------------------------------------------------------------------------
CCFWorkshopWeaponTool::CCFWorkshopWeaponTool(Panel* parent, const char* panelName)
	: BaseClass(parent, panelName)
	, m_eBrowseType(BROWSE_MODEL)
	, m_pModelPathEntry(NULL)
	, m_pBrowseModelButton(NULL)
	, m_pModelPreview(NULL)
	, m_pModelInfoLabel(NULL)
	, m_pAutoRotateCheck(NULL)
	, m_pWeaponSearchEntry(NULL)
	, m_pSlotFilterCombo(NULL)
	, m_pClassFilterCombo(NULL)
	, m_pWeaponCombo(NULL)
	, m_pWeaponIconPanel(NULL)
	, m_pWeaponIconImage(NULL)
	, m_iWeaponIconTextureID(-1)
	, m_pWeaponIconMaterial(NULL)
	, m_pReplaceViewModelCheck(NULL)
	, m_pReplaceWorldModelCheck(NULL)
	, m_pAddReplacementButton(NULL)
	, m_pRemoveReplacementButton(NULL)
	, m_pReplacementList(NULL)
	, m_pModNameEntry(NULL)
	, m_pModDescriptionEntry(NULL)
	, m_pOutputPathEntry(NULL)
	, m_pBrowseOutputButton(NULL)
	, m_pGenerateButton(NULL)
	, m_pCancelButton(NULL)
	, m_pStatusLabel(NULL)
	, m_flNextStatusClearTime(0)
{
	SetScheme(scheme()->LoadSchemeFromFileEx(enginevgui->GetPanel(PANEL_CLIENTDLL), 
		"resource/ClientScheme.res", "ClientScheme"));
	SetProportional(false);
	SetDeleteSelfOnClose(true);
	SetSizeable(false);
	SetMoveable(true);
    SetCloseButtonVisible(true);
    SetTitleBarVisible(false);
	SetSize(1200, 900);
	MoveToCenterOfScreen();
	SetPostChildPaintEnabled(true);  // Enable PostChildPaint for weapon icon drawing
}

CCFWorkshopWeaponTool::~CCFWorkshopWeaponTool() {}

void CCFWorkshopWeaponTool::ApplySchemeSettings(IScheme* pScheme)
{
	DevMsg("CCFWorkshopWeaponTool::ApplySchemeSettings - START\n");
	BaseClass::ApplySchemeSettings(pScheme);
	LoadControlSettings( RES_WEAPONTOOL );
	
	// Find controls
	m_pModelPathEntry = dynamic_cast<TextEntry*>(FindChildByName("ModelPathEntry"));
	m_pBrowseModelButton = dynamic_cast<Button*>(FindChildByName("BrowseModelButton"));
	
	// Create the model preview panel inside the placeholder container
	Panel* pPreviewContainer = FindChildByName("ModelPreviewPanel");
	if (pPreviewContainer && !m_pModelPreview)
	{
		int w, h;
		pPreviewContainer->GetSize(w, h);
		
		// Parent to the container so it draws inside it
		m_pModelPreview = new CWeaponModelPreviewPanel(pPreviewContainer, "ModelPreview");
		m_pModelPreview->SetBounds(0, 0, w, h);
		m_pModelPreview->SetVisible(true);
	}
	m_pModelInfoLabel = dynamic_cast<Label*>(FindChildByName("ModelInfoLabel"));
	m_pAutoRotateCheck = dynamic_cast<CheckButton*>(FindChildByName("AutoRotateCheck"));
	
	// New weapon selection controls
	m_pWeaponSearchEntry = dynamic_cast<TextEntry*>(FindChildByName("WeaponSearchEntry"));
	m_pSlotFilterCombo = dynamic_cast<ComboBox*>(FindChildByName("SlotFilterCombo"));
	m_pClassFilterCombo = dynamic_cast<ComboBox*>(FindChildByName("ClassFilterCombo"));
	m_pWeaponCombo = dynamic_cast<ComboBox*>(FindChildByName("WeaponCombo"));
	m_pWeaponIconPanel = FindChildByName("WeaponIconPanel");
	
	// Create the weapon icon image panel inside the icon container
	if (m_pWeaponIconPanel && !m_pWeaponIconImage)
	{
		int w, h;
		m_pWeaponIconPanel->GetSize(w, h);
		
		m_pWeaponIconImage = new ImagePanel(m_pWeaponIconPanel, "WeaponIcon");
		m_pWeaponIconImage->SetBounds(0, 0, w, h);
		m_pWeaponIconImage->SetShouldScaleImage(true);
		m_pWeaponIconImage->SetScaleAmount(1.0f);
		m_pWeaponIconImage->SetVisible(true);
	}
	
	m_pReplaceViewModelCheck = dynamic_cast<CheckButton*>(FindChildByName("ReplaceViewModelCheck"));
	m_pReplaceWorldModelCheck = dynamic_cast<CheckButton*>(FindChildByName("ReplaceWorldModelCheck"));
	m_pAddReplacementButton = dynamic_cast<Button*>(FindChildByName("AddReplacementButton"));
	m_pRemoveReplacementButton = dynamic_cast<Button*>(FindChildByName("RemoveReplacementButton"));
	m_pReplacementList = dynamic_cast<ListPanel*>(FindChildByName("ReplacementList"));
	m_pModNameEntry = dynamic_cast<TextEntry*>(FindChildByName("ModNameEntry"));
	m_pModDescriptionEntry = dynamic_cast<TextEntry*>(FindChildByName("ModDescriptionEntry"));
	m_pOutputPathEntry = dynamic_cast<TextEntry*>(FindChildByName("OutputPathEntry"));
	m_pBrowseOutputButton = dynamic_cast<Button*>(FindChildByName("BrowseOutputButton"));
	m_pGenerateButton = dynamic_cast<Button*>(FindChildByName("GenerateButton"));
	m_pCancelButton = dynamic_cast<Button*>(FindChildByName("CancelButton"));
	m_pStatusLabel = dynamic_cast<Label*>(FindChildByName("StatusLabel"));
	
	// Setup search entry
	if (m_pWeaponSearchEntry)
	{
		m_pWeaponSearchEntry->AddActionSignalTarget(this);
		m_pWeaponSearchEntry->SendNewLine(true);
	}
	
	// Setup slot filter combo
	if (m_pSlotFilterCombo)
	{
		m_pSlotFilterCombo->AddItem("All Slots", NULL);
		m_pSlotFilterCombo->AddItem("Primary", NULL);
		m_pSlotFilterCombo->AddItem("Secondary", NULL);
		m_pSlotFilterCombo->AddItem("Melee", NULL);
		m_pSlotFilterCombo->AddItem("PDA", NULL);
		m_pSlotFilterCombo->AddItem("Building", NULL);
		m_pSlotFilterCombo->ActivateItemByRow(0);
		m_pSlotFilterCombo->AddActionSignalTarget(this);
	}
	
	// Setup class filter combo
	if (m_pClassFilterCombo) {
		m_pClassFilterCombo->AddItem("All Classes", NULL);
		m_pClassFilterCombo->AddItem("Scout", NULL);
		m_pClassFilterCombo->AddItem("Sniper", NULL);
		m_pClassFilterCombo->AddItem("Soldier", NULL);
		m_pClassFilterCombo->AddItem("Demoman", NULL);
		m_pClassFilterCombo->AddItem("Medic", NULL);
		m_pClassFilterCombo->AddItem("Heavy", NULL);
		m_pClassFilterCombo->AddItem("Pyro", NULL);
		m_pClassFilterCombo->AddItem("Spy", NULL);
		m_pClassFilterCombo->AddItem("Engineer", NULL);
		m_pClassFilterCombo->ActivateItemByRow(0);
		m_pClassFilterCombo->AddActionSignalTarget(this);
	}
	
	// Setup replacement list columns
	if (m_pReplacementList) {
		m_pReplacementList->AddColumnHeader(0, "weapon", "Target Weapon", 200, ListPanel::COLUMN_RESIZEWITHWINDOW);
		m_pReplacementList->AddColumnHeader(1, "model", "Custom Model", 250, ListPanel::COLUMN_RESIZEWITHWINDOW);
		m_pReplacementList->AddColumnHeader(2, "type", "Type", 80, 0);
	}
	
	// Default checkboxes
	if (m_pReplaceViewModelCheck) m_pReplaceViewModelCheck->SetSelected(true);
	if (m_pReplaceWorldModelCheck) m_pReplaceWorldModelCheck->SetSelected(true);
	if (m_pAutoRotateCheck) m_pAutoRotateCheck->SetSelected(false);
	
	PopulateWeaponDropdown();
}

void CCFWorkshopWeaponTool::PerformLayout()
{
	BaseClass::PerformLayout();
}

void CCFWorkshopWeaponTool::PopulateWeaponDropdown()
{
	DevMsg("CCFWorkshopWeaponTool::PopulateWeaponDropdown - START\n");
	if (!m_pWeaponCombo) 
	{
		DevMsg("CCFWorkshopWeaponTool::PopulateWeaponDropdown - m_pWeaponCombo is NULL!\n");
		return;
	}
	m_pWeaponCombo->RemoveAll();
	
	int classFilter = m_pClassFilterCombo ? m_pClassFilterCombo->GetActiveItem() : 0;
	int slotFilter = m_pSlotFilterCombo ? m_pSlotFilterCombo->GetActiveItem() : 0;
	
	// Get search text
	char szSearch[256] = {0};
	if (m_pWeaponSearchEntry)
	{
		m_pWeaponSearchEntry->GetText(szSearch, sizeof(szSearch));
	}
	
	// Get all item definitions from the schema
	const CEconItemSchema::ItemDefinitionMap_t& mapItemDefs = ItemSystem()->GetItemSchema()->GetItemDefinitionMap();
	
	// Create a sorted list of weapons
	struct WeaponEntry_t
	{
		item_definition_index_t nDefIndex;
		CUtlString strDisplayName;
		int iSlot;
	};
	CUtlVector<WeaponEntry_t> vecWeapons;
	
	// Track seen weapon names to filter duplicates
	CUtlStringMap<bool> mapSeenNames;
	
	FOR_EACH_MAP_FAST(mapItemDefs, i)
	{
		CEconItemDefinition* pBaseDef = mapItemDefs[i];
		if (!pBaseDef)
			continue;
		
		// Cast to TF-specific item definition
		const CTFItemDefinition* pItemDef = dynamic_cast<const CTFItemDefinition*>(pBaseDef);
		if (!pItemDef)
			continue;
		
		// Skip hidden items
		if (pItemDef->IsHidden())
			continue;
		
		// Skip items without a valid item class (not actually spawnable weapons)
		const char* pszItemClass = pItemDef->GetItemClass();
		if (!pszItemClass || !pszItemClass[0])
			continue;
		
		// Skip non-weapon item classes
		if (Q_strnicmp(pszItemClass, "tf_weapon", 9) != 0)
			continue;
		
		// Get the loadout slot - if filtering by class, check that specific class
		int iSlot;
		if (classFilter > 0)
		{
			// Check if this weapon can be used by the selected class
			if (!pItemDef->CanBeUsedByClass(classFilter))
				continue;
			
			iSlot = pItemDef->GetLoadoutSlot(classFilter);
		}
		else
		{
			// For "All Classes", use default loadout slot
			iSlot = pItemDef->GetDefaultLoadoutSlot();
		}
		
		// Skip non-weapon slots (cosmetics, taunts, etc.)
		if (!IsWeaponLoadoutSlot(iSlot))
			continue;
		
		// Apply slot filter
		// 0 = All, 1 = Primary, 2 = Secondary, 3 = Melee, 4 = PDA, 5 = Building
		if (slotFilter > 0)
		{
			bool bMatchesSlot = false;
			switch (slotFilter)
			{
				case 1: bMatchesSlot = (iSlot == LOADOUT_POSITION_PRIMARY); break;
				case 2: bMatchesSlot = (iSlot == LOADOUT_POSITION_SECONDARY); break;
				case 3: bMatchesSlot = (iSlot == LOADOUT_POSITION_MELEE); break;
				case 4: bMatchesSlot = (iSlot == LOADOUT_POSITION_PDA || iSlot == LOADOUT_POSITION_PDA2); break;
				case 5: bMatchesSlot = (iSlot == LOADOUT_POSITION_BUILDING); break;
			}
			if (!bMatchesSlot)
				continue;
		}
		
		// Skip items without a model
		const char* pszModel = GetWeaponModelPath(pItemDef);
		if (!pszModel || !pszModel[0])
			continue;
		
		// Get display name early so we can filter by it
		const char* pszDisplayName = GetItemDisplayName(pItemDef);
		
		// Skip duplicate weapon names (keep only the first occurrence)
		if (mapSeenNames.Defined(pszDisplayName))
			continue;
		mapSeenNames[pszDisplayName] = true;
		
		// Apply search filter
		if (szSearch[0] != '\0')
		{
			if (!V_stristr(pszDisplayName, szSearch) && !V_stristr(pItemDef->GetDefinitionName(), szSearch))
				continue;
		}
		
		WeaponEntry_t entry;
		entry.nDefIndex = pItemDef->GetDefinitionIndex();
		entry.strDisplayName = pszDisplayName;
		entry.iSlot = iSlot;
		vecWeapons.AddToTail(entry);
	}
	
	// Sort weapons by slot then by name
	vecWeapons.Sort([](const WeaponEntry_t* a, const WeaponEntry_t* b) -> int
	{
		if (a->iSlot != b->iSlot)
			return a->iSlot - b->iSlot;
		return Q_stricmp(a->strDisplayName.Get(), b->strDisplayName.Get());
	});
	
	// Add sorted weapons to combo box
	FOR_EACH_VEC(vecWeapons, j)
	{
		KeyValues* pData = new KeyValues("data");
		pData->SetInt("defindex", vecWeapons[j].nDefIndex);
		m_pWeaponCombo->AddItem(vecWeapons[j].strDisplayName.Get(), pData);
	}
	
	if (m_pWeaponCombo->GetItemCount() > 0)
		m_pWeaponCombo->ActivateItemByRow(0);
	
	// Update the weapon icon for current selection
	DevMsg("CCFWorkshopWeaponTool::PopulateWeaponDropdown - Calling UpdateWeaponIcon, item count: %d\n", m_pWeaponCombo->GetItemCount());
	UpdateWeaponIcon();
}

void CCFWorkshopWeaponTool::FilterWeaponsByClass(int iClassIndex)
{
	PopulateWeaponDropdown();
}

void CCFWorkshopWeaponTool::FilterWeaponsBySlot(int iSlot)
{
	PopulateWeaponDropdown();
}

void CCFWorkshopWeaponTool::FilterWeaponsBySearch()
{
	PopulateWeaponDropdown();
}

void CCFWorkshopWeaponTool::UpdateWeaponIcon()
{
	// Reset previous state
	m_iWeaponIconTextureID = -1;
	m_pWeaponIconMaterial = NULL;
	
	if (!m_pWeaponIconPanel || !m_pWeaponCombo)
		return;
	
	// Get currently selected weapon
	if (m_pWeaponCombo->GetActiveItem() < 0)
		return;
	
	KeyValues* pData = m_pWeaponCombo->GetActiveItemUserData();
	if (!pData)
		return;
	
	item_definition_index_t nDefIndex = pData->GetInt("defindex", INVALID_ITEM_DEF_INDEX);
	if (nDefIndex == INVALID_ITEM_DEF_INDEX)
		return;
	
	const CTFItemDefinition* pItemDef = dynamic_cast<const CTFItemDefinition*>(
		ItemSystem()->GetItemSchema()->GetItemDefinition(nDefIndex));
	if (!pItemDef)
		return;
	
	// Get the inventory image path
	const char* pszInventoryImage = pItemDef->GetInventoryImage();
	if (!pszInventoryImage || !pszInventoryImage[0])
	{
		DevMsg("CCFWorkshopWeaponTool: No inventory image for item %d\n", nDefIndex);
		return;
	}
	
	DevMsg("CCFWorkshopWeaponTool: Loading inventory image '%s'\n", pszInventoryImage);
	
	// Create texture ID if needed
	m_iWeaponIconTextureID = vgui::surface()->CreateNewTextureID();
	
	// Try loading the large version first using DrawSetTextureFile
	char szMaterialPath[MAX_PATH];
	Q_snprintf(szMaterialPath, sizeof(szMaterialPath), "%s_large", pszInventoryImage);
	
	// DrawSetTextureFile takes path relative to materials/ folder (no extension)
	vgui::surface()->DrawSetTextureFile(m_iWeaponIconTextureID, szMaterialPath, true, false);
	
	// Check if texture loaded by checking dimensions
	int texW, texH;
	vgui::surface()->DrawGetTextureSize(m_iWeaponIconTextureID, texW, texH);
	
	if (texW <= 0 || texH <= 0)
	{
		// Try without _large suffix
		vgui::surface()->DrawSetTextureFile(m_iWeaponIconTextureID, pszInventoryImage, true, false);
		vgui::surface()->DrawGetTextureSize(m_iWeaponIconTextureID, texW, texH);
	}
	
	if (texW > 0 && texH > 0)
	{
		DevMsg("CCFWorkshopWeaponTool: Texture loaded, size %dx%d\n", texW, texH);
	}
	else
	{
		DevMsg("CCFWorkshopWeaponTool: Failed to load texture for '%s'\n", pszInventoryImage);
		m_iWeaponIconTextureID = -1;
	}
	
	// Force a repaint
	Repaint();
}

void CCFWorkshopWeaponTool::OnTextChanged(KeyValues* data)
{
	Panel* pPanel = (Panel*)data->GetPtr("panel");
	if (pPanel == m_pWeaponSearchEntry)
	{
		PopulateWeaponDropdown();
	}
	else if (pPanel == m_pSlotFilterCombo || pPanel == m_pClassFilterCombo)
	{
		PopulateWeaponDropdown();
	}
	else if (pPanel == m_pWeaponCombo)
	{
		UpdateWeaponIcon();
	}
}

void CCFWorkshopWeaponTool::OnCommand(const char* command)
{
	if (Q_stricmp(command, "BrowseModel") == 0) {
		OpenModelFileBrowser();
	} else if (Q_stricmp(command, "BrowseOutput") == 0) {
		OpenOutputDirectoryBrowser();
	} else if (Q_stricmp(command, "AddWeapon") == 0 || Q_stricmp(command, "AddReplacement") == 0) {
		AddModelReplacement();
	} else if (Q_stricmp(command, "RemoveWeapon") == 0 || Q_stricmp(command, "RemoveReplacement") == 0) {
		RemoveSelectedReplacement();
	} else if (Q_stricmp(command, "CreateMod") == 0 || Q_stricmp(command, "Generate") == 0) {
		GenerateWeaponMod();
	} else if (Q_stricmp(command, "ClearList") == 0) {
		ClearAllReplacements();
	} else if (Q_stricmp(command, "ResetView") == 0) {
		if (m_pModelPreview)
			m_pModelPreview->ResetCamera();
	} else if (Q_stricmp(command, "Cancel") == 0 || Q_stricmp(command, "Close") == 0) {
		Close();
	} else if (Q_stricmp(command, "ClassFilterChanged") == 0) {
		PopulateWeaponDropdown();
	} else if (Q_stricmp(command, "ToggleAutoRotate") == 0) {
		if (m_pModelPreview && m_pAutoRotateCheck)
			m_pModelPreview->SetAutoRotate(m_pAutoRotateCheck->IsSelected());
	} else {
		BaseClass::OnCommand(command);
	}
}

void CCFWorkshopWeaponTool::OnThink()
{
	BaseClass::OnThink();
	if (m_flNextStatusClearTime > 0 && gpGlobals->curtime > m_flNextStatusClearTime) {
		if (m_pStatusLabel) m_pStatusLabel->SetText("");
		m_flNextStatusClearTime = 0;
	}
}

void CCFWorkshopWeaponTool::PostChildPaint()
{
	BaseClass::PostChildPaint();
	
	// Draw the weapon icon if we have one (after children so it's on top)
	if (m_iWeaponIconTextureID != -1 && m_pWeaponIconPanel)
	{
		// Get the icon panel's position relative to this frame
		int panelX, panelY;
		m_pWeaponIconPanel->GetPos(panelX, panelY);
		int panelW, panelH;
		m_pWeaponIconPanel->GetSize(panelW, panelH);
		
		// Get the texture dimensions for aspect ratio calculation
		int texW, texH;
		vgui::surface()->DrawGetTextureSize(m_iWeaponIconTextureID, texW, texH);
		
		static bool bLoggedOnce = false;
		if (!bLoggedOnce)
		{
			DevMsg("CCFWorkshopWeaponTool::PostChildPaint - Drawing at (%d,%d) size %dx%d, tex size %dx%d, texID %d\n", 
				panelX, panelY, panelW, panelH, texW, texH, m_iWeaponIconTextureID);
			bLoggedOnce = true;
		}
		
		// Calculate aspect-correct size
		int drawW = panelW;
		int drawH = panelH;
		int drawX = panelX;
		int drawY = panelY;
		
		if (texW > 0 && texH > 0)
		{
			float texAspect = (float)texW / (float)texH;
			float panelAspect = (float)panelW / (float)panelH;
			
			if (texAspect > panelAspect)
			{
				// Texture is wider, fit to width
				drawH = (int)(panelW / texAspect);
				drawY = panelY + (panelH - drawH) / 2;
			}
			else
			{
				// Texture is taller, fit to height
				drawW = (int)(panelH * texAspect);
				drawX = panelX + (panelW - drawW) / 2;
			}
		}
		
		// Draw the weapon icon texture
		vgui::surface()->DrawSetTexture(m_iWeaponIconTextureID);
		vgui::surface()->DrawSetColor(255, 255, 255, 255);
		vgui::surface()->DrawTexturedRect(drawX, drawY, drawX + drawW, drawY + drawH);
	}
}

void CCFWorkshopWeaponTool::OnKeyCodePressed(KeyCode code)
{
	if (code == KEY_ESCAPE) Close();
	else BaseClass::OnKeyCodePressed(code);
}

void CCFWorkshopWeaponTool::ShowPanel(bool bShow)
{
	SetVisible(bShow);
	if (bShow) { MoveToFront(); RequestFocus(); }
}

void CCFWorkshopWeaponTool::OpenModelFileBrowser()
{
	m_eBrowseType = BROWSE_MODEL;
	FileOpenDialog* pDialog = new FileOpenDialog(this, "Select Model File", true);
	pDialog->AddFilter("*.mdl", "Model Files (*.mdl)", true);
	pDialog->AddActionSignalTarget(this);
	pDialog->DoModal();
}

void CCFWorkshopWeaponTool::OpenOutputDirectoryBrowser()
{
	m_eBrowseType = BROWSE_OUTPUT;
	DirectorySelectDialog* pDialog = new DirectorySelectDialog(this, "Select Output Directory");
	pDialog->AddActionSignalTarget(this);
	pDialog->MakeReadyForUse();
	
	// Set start directory to current output path or current directory
	char szStartDir[MAX_PATH];
	if (m_strOutputPath.Length() > 0)
	{
		Q_strncpy(szStartDir, m_strOutputPath.Get(), sizeof(szStartDir));
	}
	else
	{
		g_pFullFileSystem->GetCurrentDirectory(szStartDir, sizeof(szStartDir));
	}
	pDialog->SetStartDirectory(szStartDir);
	pDialog->DoModal();
}

void CCFWorkshopWeaponTool::OnFileSelected(KeyValues* params)
{
	const char* pszPath = params->GetString("fullpath");
	if (!pszPath || !pszPath[0]) return;
	
	if (m_eBrowseType == BROWSE_MODEL) {
		m_strCurrentModelPath = pszPath;
		if (m_pModelPathEntry) m_pModelPathEntry->SetText(pszPath);
		UpdateModelPreview();
	}
}

void CCFWorkshopWeaponTool::OnDirectorySelected(KeyValues* params)
{
	const char* pszPath = params->GetString("dir");
	if (!pszPath || !pszPath[0]) return;
	
	m_strOutputPath = pszPath;
	if (m_pOutputPathEntry) m_pOutputPathEntry->SetText(pszPath);
}

void CCFWorkshopWeaponTool::OnItemSelected() {}
void CCFWorkshopWeaponTool::OnItemDoubleClicked(int itemID) {}

void CCFWorkshopWeaponTool::UpdateModelPreview()
{
	if (!m_pModelPreview || m_strCurrentModelPath.IsEmpty()) return;
	
	const char* pszPath = m_strCurrentModelPath.Get();
	
	// Check if this is a valid model path
	const char* pModelsDir = V_stristr(pszPath, "models\\");
	if (!pModelsDir)
		pModelsDir = V_stristr(pszPath, "models/");
	
	if (!pModelsDir)
	{
		SetStatus("Error: Model must be inside a 'models' folder!");
		return;
	}
	
	if (m_pModelPreview->LoadModel(pszPath)) {
		SetStatus("Model loaded: %s", V_GetFileName(pszPath));
		if (m_pModelInfoLabel)
			m_pModelInfoLabel->SetText(V_GetFileName(pszPath));
	} else {
		SetStatus("Failed to load model - check console for details");
	}
}

void CCFWorkshopWeaponTool::AddModelReplacement()
{
	if (m_strCurrentModelPath.IsEmpty()) {
		SetStatus("Please select a model file first!");
		return;
	}
	if (!m_pWeaponCombo || m_pWeaponCombo->GetActiveItem() < 0) {
		SetStatus("Please select a target weapon!");
		return;
	}
	
	KeyValues* pData = m_pWeaponCombo->GetActiveItemUserData();
	if (!pData) return;
	
	// Get item definition from schema using the stored definition index
	item_definition_index_t nDefIndex = pData->GetInt("defindex", INVALID_ITEM_DEF_INDEX);
	if (nDefIndex == INVALID_ITEM_DEF_INDEX)
		return;
	
	const CTFItemDefinition* pItemDef = dynamic_cast<const CTFItemDefinition*>(
		ItemSystem()->GetItemSchema()->GetItemDefinition(nDefIndex));
	if (!pItemDef)
		return;
	
	const char* pszWeaponClass = pItemDef->GetItemClass();
	const char* pszModelPath = GetWeaponModelPath(pItemDef);
	const char* pszDisplayName = GetItemDisplayName(pItemDef);
	
	if (!pszModelPath || !pszModelPath[0])
	{
		SetStatus("Error: Selected weapon has no model path!");
		return;
	}
	
	// Add to our data
	WeaponReplacementEntry_t entry;
	entry.strCustomModelPath = m_strCurrentModelPath;
	entry.strTargetWeaponClass = pszWeaponClass;
	entry.strTargetModelPath = pszModelPath;
	entry.strDisplayName = pszDisplayName;
	entry.bReplaceViewModel = true;
	entry.bReplaceWorldModel = true;
	m_vecReplacements.AddToTail(entry);
	
	// Add to list panel
	if (m_pReplacementList) {
		KeyValues* pKV = new KeyValues("item");
		pKV->SetString("weapon", pszDisplayName);
		pKV->SetString("model", V_GetFileName(m_strCurrentModelPath.Get()));
		pKV->SetString("type", "C_Model");
		m_pReplacementList->AddItem(pKV, 0, false, false);
		pKV->deleteThis();
	}
	
	SetStatus("Added replacement: %s -> %s", pszDisplayName, V_GetFileName(m_strCurrentModelPath.Get()));
}

void CCFWorkshopWeaponTool::RemoveSelectedReplacement()
{
	if (!m_pReplacementList) return;
	int sel = m_pReplacementList->GetSelectedItem(0);
	if (sel < 0) return;
	
	int dataIdx = m_pReplacementList->GetItemCurrentRow(sel);
	if (dataIdx >= 0 && dataIdx < m_vecReplacements.Count())
		m_vecReplacements.Remove(dataIdx);
	
	m_pReplacementList->RemoveItem(sel);
	SetStatus("Replacement removed");
}

void CCFWorkshopWeaponTool::ClearAllReplacements()
{
	m_vecReplacements.RemoveAll();
	if (m_pReplacementList) m_pReplacementList->RemoveAll();
}

void CCFWorkshopWeaponTool::SetStatus(const char* pszFormat, ...)
{
	char szBuffer[512];
	va_list args;
	va_start(args, pszFormat);
	V_vsnprintf(szBuffer, sizeof(szBuffer), pszFormat, args);
	va_end(args);
	
	if (m_pStatusLabel) m_pStatusLabel->SetText(szBuffer);
	m_flNextStatusClearTime = gpGlobals->curtime + 5.0f;
}

bool CCFWorkshopWeaponTool::ValidateModelFile(const char* pszModelPath)
{
	return pszModelPath && g_pFullFileSystem->FileExists(pszModelPath);
}

void CCFWorkshopWeaponTool::GenerateWeaponMod()
{
	if (m_vecReplacements.Count() == 0) {
		SetStatus("No weapon replacements defined!");
		return;
	}
	
	if (m_strOutputPath.IsEmpty()) {
		SetStatus("Please select an output directory!");
		return;
	}
	
	int nSuccessCount = 0;
	
	// Process each replacement
	for (int i = 0; i < m_vecReplacements.Count(); i++)
	{
		const WeaponReplacementEntry_t& entry = m_vecReplacements[i];
		if (entry.strTargetModelPath.IsEmpty())
			continue;
		
		// Copy to the target model path
		if (CopyModelToTarget(entry.strCustomModelPath.Get(), entry.strTargetModelPath.Get()))
			nSuccessCount++;
	}
	
	if (nSuccessCount > 0)
	{
		SetStatus("Created %d model replacement(s) at: %s", nSuccessCount, m_strOutputPath.Get());
		
		// Show success message
		char szMsg[512];
		V_snprintf(szMsg, sizeof(szMsg), "Successfully created %d model file(s)!\n\nOutput: %s", nSuccessCount, m_strOutputPath.Get());
		MessageBox* pMsg = new MessageBox("Success", szMsg, this);
		pMsg->DoModal();
	}
	else
	{
		SetStatus("Failed to create any model files!");
	}
}

bool CCFWorkshopWeaponTool::CopyModelToTarget(const char* pszSourcePath, const char* pszTargetModelPath)
{
	if (!pszSourcePath || !pszSourcePath[0] || !pszTargetModelPath || !pszTargetModelPath[0])
		return false;
	
	// Clean up the output path - remove trailing slashes
	char szOutputDir[MAX_PATH];
	V_strcpy_safe(szOutputDir, m_strOutputPath.Get());
	V_StripTrailingSlash(szOutputDir);
	
	// Get source and dest base paths (without .mdl extension)
	char szSourceBase[MAX_PATH];
	char szDestBase[MAX_PATH];
	
	V_strcpy_safe(szSourceBase, pszSourcePath);
	V_StripExtension(szSourceBase, szSourceBase, sizeof(szSourceBase));
	
	V_snprintf(szDestBase, sizeof(szDestBase), "%s%c%s", szOutputDir, CORRECT_PATH_SEPARATOR, pszTargetModelPath);
	V_StripExtension(szDestBase, szDestBase, sizeof(szDestBase));
	V_FixSlashes(szDestBase);
	
	// Create the directory hierarchy for the destination
	char szDestDir[MAX_PATH];
	V_strcpy_safe(szDestDir, szDestBase);
	V_StripFilename(szDestDir);
	
	// Create directories recursively
	CUtlVector<CUtlString> dirsToCreate;
	char szCheckDir[MAX_PATH];
	V_strcpy_safe(szCheckDir, szDestDir);
	
	while (szCheckDir[0] && !g_pFullFileSystem->IsDirectory(szCheckDir))
	{
		dirsToCreate.AddToHead(szCheckDir);
		V_StripLastDir(szCheckDir, sizeof(szCheckDir));
		V_StripTrailingSlash(szCheckDir);
	}
	
	for (int i = 0; i < dirsToCreate.Count(); i++)
	{
#ifdef _WIN32
		_mkdir(dirsToCreate[i].Get());
#else
		mkdir(dirsToCreate[i].Get(), 0755);
#endif
	}
	
	// Get source full path base
	char szSourceFullBase[MAX_PATH];
	if (V_IsAbsolutePath(szSourceBase))
	{
		V_strcpy_safe(szSourceFullBase, szSourceBase);
	}
	else
	{
		char szTemp[MAX_PATH];
		V_snprintf(szTemp, sizeof(szTemp), "%s.mdl", szSourceBase);
		g_pFullFileSystem->RelativePathToFullPath(szTemp, "GAME", szSourceFullBase, sizeof(szSourceFullBase));
		V_StripExtension(szSourceFullBase, szSourceFullBase, sizeof(szSourceFullBase));
	}
	V_FixSlashes(szSourceFullBase);
	
	// List of model file extensions to copy
	const char* szExtensions[] = {
		".mdl",
		".vvd",
		".phy",
		".dx90.vtx",
		".dx80.vtx",
		".sw.vtx",
	};
	
	int nFilesCopied = 0;
	
	for (int i = 0; i < ARRAYSIZE(szExtensions); i++)
	{
		char szSrcFile[MAX_PATH];
		char szDstFile[MAX_PATH];
		
		V_snprintf(szSrcFile, sizeof(szSrcFile), "%s%s", szSourceFullBase, szExtensions[i]);
		V_snprintf(szDstFile, sizeof(szDstFile), "%s%s", szDestBase, szExtensions[i]);
		
		// Check if source file exists
		FILE* pSrcFile = fopen(szSrcFile, "rb");
		if (!pSrcFile)
		{
			// .phy is optional, others should warn
			if (Q_stricmp(szExtensions[i], ".phy") != 0)
			{
				Warning("CopyModelToTarget: Source file not found: %s\n", szSrcFile);
			}
			continue;
		}
		
		// Get file size
		fseek(pSrcFile, 0, SEEK_END);
		long nFileSize = ftell(pSrcFile);
		fseek(pSrcFile, 0, SEEK_SET);
		
		// Read file contents
		unsigned char* pBuffer = new unsigned char[nFileSize];
		size_t nRead = fread(pBuffer, 1, nFileSize, pSrcFile);
		fclose(pSrcFile);
		
		if (nRead != (size_t)nFileSize)
		{
			delete[] pBuffer;
			Warning("CopyModelToTarget: Failed to read file: %s\n", szSrcFile);
			continue;
		}
		
		// Write to destination
		FILE* pDstFile = fopen(szDstFile, "wb");
		if (!pDstFile)
		{
			delete[] pBuffer;
			Warning("CopyModelToTarget: Failed to create file: %s\n", szDstFile);
			continue;
		}
		
		size_t nWritten = fwrite(pBuffer, 1, nFileSize, pDstFile);
		fclose(pDstFile);
		delete[] pBuffer;
		
		if (nWritten == (size_t)nFileSize)
		{
			Msg("CopyModelToTarget: Copied %s\n", szExtensions[i]);
			nFilesCopied++;
		}
	}
	
	Msg("CopyModelToTarget: Copied %d files for %s\n", nFilesCopied, V_GetFileName(pszTargetModelPath));
	return nFilesCopied > 0;
}

bool CCFWorkshopWeaponTool::CopyModelFiles(const char* pszSourcePath, const char* pszDestPath)
{
	// This function is now deprecated - use CopyModelToTarget instead
	return false;
}

bool CCFWorkshopWeaponTool::GenerateWeaponScript(const char* pszOutputPath)
{
	// Disabled for now
	return true;
}

//-----------------------------------------------------------------------------
// Console command
//-----------------------------------------------------------------------------
CON_COMMAND(cf_workshop_weapon_tool, "Open the Workshop Weapon Mod Tool")
{
	CCFWorkshopWeaponTool* pTool = new CCFWorkshopWeaponTool(NULL, "WorkshopWeaponTool");
	pTool->ShowPanel(true);
	pTool->MakePopup();
}

void OpenWorkshopWeaponTool()
{
	engine->ClientCmd_Unrestricted("cf_workshop_weapon_tool");
}

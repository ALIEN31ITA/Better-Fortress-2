//====== Copyright Custom Fortress 2, All rights reserved. =================
//
// Workshop Cosmetic Preview Tool Implementation
//
//=============================================================================

#include "cbase.h"
#include "cf_workshop_cosmetic_tool.h"
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
#include "tf_shareddefs.h"
#include "tf_classdata.h"
#include "econ_item_system.h"
#include "econ_item_schema.h"
#include "game_item_schema.h"
#include "tf_item_schema.h"
#include "tf_item_inventory.h"
#include <stdlib.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#endif
#include <stdio.h>

#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Class names for dropdown
//-----------------------------------------------------------------------------
static const char* g_szClassNames[] = {
	"Scout",
	"Soldier", 
	"Pyro",
	"Demoman",
	"Heavy",
	"Engineer",
	"Medic",
	"Sniper",
	"Spy"
};
static const int g_ClassIndices[] = {
	TF_CLASS_SCOUT,
	TF_CLASS_SOLDIER,
	TF_CLASS_PYRO,
	TF_CLASS_DEMOMAN,
	TF_CLASS_HEAVYWEAPONS,
	TF_CLASS_ENGINEER,
	TF_CLASS_MEDIC,
	TF_CLASS_SNIPER,
	TF_CLASS_SPY
};
static const int g_nClassCount = ARRAYSIZE(g_szClassNames);

//-----------------------------------------------------------------------------
// CCosmeticPreviewPanel - Extends CTFPlayerModelPanel with mouse manipulation
//-----------------------------------------------------------------------------
CCosmeticPreviewPanel::CCosmeticPreviewPanel(Panel* parent, const char* panelName)
	: BaseClass(parent, panelName)
	, m_bAutoRotate(false)
	, m_flAutoRotateSpeed(30.0f)
	, m_bNeedsAnimationSetup(true)
	, m_flAnimationPlaybackRate(1.0f)
	, m_flAnimationTime(0.0f)
	, m_flLastTickTime(0.0f)
{	
	// Enable mouse manipulation from CBaseModelPanel
	m_bAllowRotation = true;
	m_bAllowPitch = true;
	m_bAllowFullManipulation = true;
	SetMouseInputEnabled(true);
}

void CCosmeticPreviewPanel::SetupDefaultWeaponPose()
{
	// Mark that we need animation setup - it will be done in OnThink when the model is ready
	// Start with reference pose (sequence 0)
	m_bNeedsAnimationSetup = true;
}

void CCosmeticPreviewPanel::SetAnimationSequence(int iSequence)
{
	if (!m_RootMDL.m_pStudioHdr)
		return;
	
	SetSequence(iSequence, true);
}

void CCosmeticPreviewPanel::SetAnimationPlaybackRate(float flRate)
{
	// Clamp rate between 0 (paused) and 10 (10x speed)
	m_flAnimationPlaybackRate = clamp(flRate, 0.0f, 10.0f);
}

int CCosmeticPreviewPanel::GetSequenceCount()
{
	if (!m_RootMDL.m_pStudioHdr)
		return 0;
	
	return m_RootMDL.m_pStudioHdr->GetNumSeq();
}

const char* CCosmeticPreviewPanel::GetSequenceName(int iSequence)
{
	if (!m_RootMDL.m_pStudioHdr)
		return "";
	
	if (iSequence < 0 || iSequence >= m_RootMDL.m_pStudioHdr->GetNumSeq())
		return "";
	
	return m_RootMDL.m_pStudioHdr->pSeqdesc(iSequence).pszLabel();
}

int CCosmeticPreviewPanel::GetBodygroupCount()
{
	if (!m_RootMDL.m_pStudioHdr)
		return 0;
	
	return m_RootMDL.m_pStudioHdr->numbodyparts();
}

const char* CCosmeticPreviewPanel::GetBodygroupName(int iGroup)
{
	if (!m_RootMDL.m_pStudioHdr)
		return "";
	
	if (iGroup < 0 || iGroup >= m_RootMDL.m_pStudioHdr->numbodyparts())
		return "";
	
	mstudiobodyparts_t* pBodypart = m_RootMDL.m_pStudioHdr->pBodypart(iGroup);
	return pBodypart->pszName();
}

int CCosmeticPreviewPanel::GetBodygroupValue(int iGroup)
{
	if (!m_RootMDL.m_pStudioHdr)
		return 0;
	
	if (iGroup < 0 || iGroup >= m_RootMDL.m_pStudioHdr->numbodyparts())
		return 0;
	
	// Extract bodygroup value from packed m_nBody
	mstudiobodyparts_t* pBodypart = m_RootMDL.m_pStudioHdr->pBodypart(iGroup);
	int iValue = (m_RootMDL.m_MDL.m_nBody / pBodypart->base) % pBodypart->nummodels;
	return iValue;
}

void CCosmeticPreviewPanel::SetBodygroupValue(int iGroup, int iValue)
{
	if (!m_RootMDL.m_pStudioHdr)
		return;
	
	if (iGroup < 0 || iGroup >= m_RootMDL.m_pStudioHdr->numbodyparts())
		return;
	
	mstudiobodyparts_t* pBodypart = m_RootMDL.m_pStudioHdr->pBodypart(iGroup);
	if (iValue < 0 || iValue >= pBodypart->nummodels)
		return;
	
	// Pack bodygroup value into m_nBody
	m_RootMDL.m_MDL.m_nBody = (m_RootMDL.m_MDL.m_nBody - (GetBodygroupValue(iGroup) * pBodypart->base)) + (iValue * pBodypart->base);
}

int CCosmeticPreviewPanel::GetBodygroupNumModels(int iGroup)
{
	if (!m_RootMDL.m_pStudioHdr)
		return 0;
	
	if (iGroup < 0 || iGroup >= m_RootMDL.m_pStudioHdr->numbodyparts())
		return 0;
	
	mstudiobodyparts_t* pBodypart = m_RootMDL.m_pStudioHdr->pBodypart(iGroup);
	return pBodypart->nummodels;
}

CCosmeticPreviewPanel::~CCosmeticPreviewPanel()
{
	// Unmount any search paths
	UnmountCosmeticSearchPath();
}

bool CCosmeticPreviewPanel::LoadCosmeticModel(const char* pszModelPath)
{
	if (!pszModelPath || !pszModelPath[0])
		return false;
	
	// Try to extract a relative path from an absolute path
	char szRelativePath[MAX_PATH];
	
	if (g_pFullFileSystem->FileExists(pszModelPath, "GAME"))
	{
		Q_strncpy(szRelativePath, pszModelPath, sizeof(szRelativePath));
	}
	else
	{
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
			return false;
		}
	}
	
	if (!g_pFullFileSystem->FileExists(szRelativePath, "GAME"))
	{
		return false;
	}
	
	// Mount the cosmetic's directory for material loading
	MountCosmeticSearchPath(pszModelPath);
	
	m_strCosmeticModelPath = szRelativePath;
	
	// Clear existing merged models before adding new ones
	ClearMergeMDLs();
	
	// Add the cosmetic as a merge model
	int iSkin = GetTeam() == TF_TEAM_RED ? 0 : 1;
	MDLHandle_t hMDL = SetMergeMDL(szRelativePath, NULL, iSkin);
	if (hMDL == MDLHANDLE_INVALID)
	{
		return false;
	}
	
	// Set standing animation after loading models
	SetupDefaultWeaponPose();
	
	return true;
}

void CCosmeticPreviewPanel::ClearCosmeticModel()
{
	// Unmount search path first
	UnmountCosmeticSearchPath();
	
	m_strCosmeticModelPath = "";
	
	// Clear merged models and reset to standing animation
	ClearMergeMDLs();
	SetupDefaultWeaponPose();
}

void CCosmeticPreviewPanel::SetPreviewClass(int iClass)
{
	if (iClass < TF_FIRST_NORMAL_CLASS || iClass >= TF_LAST_NORMAL_CLASS)
		return;
	
	// Store current cosmetic path
	CUtlString strCosmeticPath = m_strCosmeticModelPath;
	
	// Clear merge models before changing class
	ClearMergeMDLs();
	
	// Set the new class
	SetToPlayerClass(iClass, true);
	
	// Ensure the player model has a proper standing animation
	SetupDefaultWeaponPose();
	
	// Reload the cosmetic if we had one
	if (!strCosmeticPath.IsEmpty())
	{
		LoadCosmeticModel(strCosmeticPath.Get());
	}
}

void CCosmeticPreviewPanel::SetPreviewTeam(int iTeam)
{
	if (iTeam != TF_TEAM_RED && iTeam != TF_TEAM_BLUE)
		return;
	
	// Store current cosmetic path
	CUtlString strCosmeticPath = m_strCosmeticModelPath;
	
	// Clear merge models before changing team
	ClearMergeMDLs();
	
	// Set the new team
	SetTeam(iTeam);
	
	// Ensure the player model has a proper standing animation
	SetupDefaultWeaponPose();
	
	// Reload the cosmetic if we had one (to apply team skin)
	if (!strCosmeticPath.IsEmpty())
	{
		LoadCosmeticModel(strCosmeticPath.Get());
	}
	
	UpdatePreviewVisuals();
}

void CCosmeticPreviewPanel::SetAutoRotate(bool bAutoRotate)
{
	m_bAutoRotate = bAutoRotate;
}

void CCosmeticPreviewPanel::ResetCamera()
{
	// Use the base class zoom toggle to reset
	if (IsZoomed())
	{
		ToggleZoom();
	}
	
	// Reset rotation using base class members
	m_BMPResData.m_angModelPoseRot.Init();
	m_BMPResData.m_vecOriginOffset.Init();
	m_BMPResData.m_vecFramedOriginOffset.Init();
	m_BMPResData.m_vecViewportOffset.Init();
	
	LookAtMDL();
}

void CCosmeticPreviewPanel::OnThink()
{
	BaseClass::OnThink();
	
	// Lazy animation setup - do it here once the model is fully loaded
	if (m_bNeedsAnimationSetup && m_RootMDL.m_pStudioHdr)
	{
		m_bNeedsAnimationSetup = false;
		
		// Don't force any sequence on startup - let base class handle it
		// User can select sequence 0 (reference pose) from dropdown if desired
	}
	
	// Auto-rotate when enabled and not being manipulated
	if (m_bAutoRotate && !m_bMousePressed)
	{
		RotateYaw(gpGlobals->frametime * m_flAutoRotateSpeed);
	}
}

void CCosmeticPreviewPanel::OnTick()
{
	// Custom tick handling for playback rate control
	// We need to manually control m_flTime to respect our playback rate
	
	float flCurrentTime = Plat_FloatTime();
	
	if (m_flLastTickTime == 0.0f)
	{
		m_flLastTickTime = flCurrentTime;
	}
	
	float flDeltaTime = flCurrentTime - m_flLastTickTime;
	m_flLastTickTime = flCurrentTime;
	
	// Accumulate animation time based on playback rate
	m_flAnimationTime += flDeltaTime * m_flAnimationPlaybackRate;
	
	// Call base class tick (this will set m_flTime, but we'll override it)
	BaseClass::OnTick();
	
	// Override the time with our scaled time
	if (m_RootMDL.m_MDL.GetMDL() != MDLHANDLE_INVALID)
	{
		m_RootMDL.m_MDL.m_flTime = m_flAnimationTime;
	}
}

void CCosmeticPreviewPanel::ApplySchemeSettings(IScheme* pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetPaintBackgroundEnabled(true);
	SetPaintBackgroundType(0);
	SetBackgroundColor(Color(30, 28, 27, 255));
}

void CCosmeticPreviewPanel::PaintBackground()
{
	int wide, tall;
	GetSize(wide, tall);
	
	surface()->DrawSetColor(Color(30, 28, 27, 255));
	surface()->DrawFilledRect(0, 0, wide, tall);
}

void CCosmeticPreviewPanel::OnMousePressed(MouseCode code)
{
	BaseClass::OnMousePressed(code);
}

void CCosmeticPreviewPanel::OnMouseReleased(MouseCode code)
{
	BaseClass::OnMouseReleased(code);
}

void CCosmeticPreviewPanel::OnCursorMoved(int x, int y)
{
	BaseClass::OnCursorMoved(x, y);
}

void CCosmeticPreviewPanel::OnMouseWheeled(int delta)
{
	BaseClass::OnMouseWheeled(delta);
}

void CCosmeticPreviewPanel::MountCosmeticSearchPath(const char* pszModelPath)
{
	// Unmount any previously mounted path
	UnmountCosmeticSearchPath();
	
	if (!pszModelPath || !pszModelPath[0])
		return;
	
	// We need to find the root directory that contains the "models" folder
	// For example: E:\SteamLibrary\...\workshop\content\440\123456\models\...
	// We want to mount: E:\SteamLibrary\...\workshop\content\440\123456\
	
	char szFullPath[MAX_PATH];
	Q_strncpy(szFullPath, pszModelPath, sizeof(szFullPath));
	
	// If this is an absolute path, we need to find the root to mount
	if (V_IsAbsolutePath(szFullPath))
	{
		// Look for "models" or "models\" or "models/" in the path
		char* pModelsDir = V_stristr(szFullPath, "models\\");
		if (!pModelsDir)
			pModelsDir = V_stristr(szFullPath, "models/");
		
		if (pModelsDir)
		{
			// Terminate the string at "models" to get the root directory
			*pModelsDir = '\0';
			
			// Store and mount the root path
			m_strMountedSearchPath = szFullPath;
			
			// Add to search paths at the head so it takes priority
			g_pFullFileSystem->AddSearchPath(m_strMountedSearchPath.Get(), "GAME", PATH_ADD_TO_HEAD);
			g_pFullFileSystem->AddSearchPath(m_strMountedSearchPath.Get(), "mod", PATH_ADD_TO_HEAD);
		}
	}
}

void CCosmeticPreviewPanel::UnmountCosmeticSearchPath()
{
	if (m_strMountedSearchPath.IsEmpty())
		return;
	
	// Remove from search paths
	g_pFullFileSystem->RemoveSearchPath(m_strMountedSearchPath.Get(), "GAME");
	g_pFullFileSystem->RemoveSearchPath(m_strMountedSearchPath.Get(), "mod");
	
	m_strMountedSearchPath = "";
}

//-----------------------------------------------------------------------------
// CCFWorkshopCosmeticTool
//-----------------------------------------------------------------------------
CCFWorkshopCosmeticTool::CCFWorkshopCosmeticTool(Panel* parent, const char* panelName)
	: BaseClass(parent, panelName)
	, m_pModelPathEntry(NULL)
	, m_pBrowseModelButton(NULL)
	, m_pCosmeticPreview(NULL)
	, m_pModelInfoLabel(NULL)
	, m_pClassCombo(NULL)
	, m_pTeamCombo(NULL)
	, m_pSequenceCombo(NULL)
	, m_pAnimSpeedSlider(NULL)
	, m_pAnimSpeedLabel(NULL)
	, m_pBodygroupList(NULL)
	, m_pToggleBodygroupButton(NULL)
	, m_pAutoRotateCheck(NULL)
	, m_pResetViewButton(NULL)
	, m_pApplyButton(NULL)
	, m_pCosmeticSearchEntry(NULL)
	, m_pCosmeticSlotFilterCombo(NULL)
	, m_pCosmeticCombo(NULL)
	, m_pBrowseTargetModelButton(NULL)
	, m_pAddReplacementButton(NULL)
	, m_pRemoveReplacementButton(NULL)
	, m_pReplacementList(NULL)
	, m_pOutputPathEntry(NULL)
	, m_pBrowseOutputButton(NULL)
	, m_pUseTF2FormatCheck(NULL)
	, m_pModNameEntry(NULL)
	, m_pModDescriptionEntry(NULL)
	, m_pGenerateButton(NULL)
	, m_pCloseButton(NULL)
	, m_pStatusLabel(NULL)
	, m_eBrowseType(BROWSE_MODEL)
	, m_flNextStatusClearTime(0)
	, m_bNeedsSequenceDropdownRefresh(false)
	, m_bModelInitialized(false)
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
}

CCFWorkshopCosmeticTool::~CCFWorkshopCosmeticTool() {}

void CCFWorkshopCosmeticTool::ApplySchemeSettings(IScheme* pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	LoadControlSettings("resource/UI/WorkshopCosmeticTool.res");
	
	// Find controls
	m_pModelPathEntry = dynamic_cast<TextEntry*>(FindChildByName("ModelPathEntry"));
	m_pBrowseModelButton = dynamic_cast<Button*>(FindChildByName("BrowseModelButton"));
	m_pModelInfoLabel = dynamic_cast<Label*>(FindChildByName("ModelInfoLabel"));
	
	// Create the cosmetic preview panel inside the placeholder container
	Panel* pPreviewContainer = FindChildByName("CosmeticPreviewPanel");
	if (pPreviewContainer && !m_pCosmeticPreview)
	{
		int w, h;
		pPreviewContainer->GetSize(w, h);
		
		m_pCosmeticPreview = new CCosmeticPreviewPanel(pPreviewContainer, "PreviewModel");
		m_pCosmeticPreview->SetBounds(0, 0, w, h);
		m_pCosmeticPreview->SetVisible(true);
	}
	
	// Class/Team selection
	m_pClassCombo = dynamic_cast<ComboBox*>(FindChildByName("ClassCombo"));
	m_pTeamCombo = dynamic_cast<ComboBox*>(FindChildByName("TeamCombo"));
	
	// Animation controls
	m_pSequenceCombo = dynamic_cast<ComboBox*>(FindChildByName("SequenceCombo"));
	m_pAnimSpeedSlider = dynamic_cast<Slider*>(FindChildByName("AnimSpeedSlider"));
	m_pAnimSpeedLabel = dynamic_cast<Label*>(FindChildByName("AnimSpeedLabel"));
	
	// Bodygroup controls
	m_pBodygroupList = dynamic_cast<ListPanel*>(FindChildByName("BodygroupList"));
	m_pToggleBodygroupButton = dynamic_cast<Button*>(FindChildByName("ToggleBodygroupButton"));
	
	if (m_pBodygroupList)
	{
		m_pBodygroupList->AddColumnHeader(0, "bodygroup", "Bodygroup", 180, ListPanel::COLUMN_RESIZEWITHWINDOW);
		m_pBodygroupList->AddColumnHeader(1, "value", "Value", 80, ListPanel::COLUMN_FIXEDSIZE);
		m_pBodygroupList->SetSortColumn(0);
		m_pBodygroupList->SetMultiselectEnabled(false);
	}
	
	// View controls
	m_pAutoRotateCheck = dynamic_cast<CheckButton*>(FindChildByName("AutoRotateCheck"));
	m_pResetViewButton = dynamic_cast<Button*>(FindChildByName("ResetViewButton"));
	
	// Apply button
	m_pApplyButton = dynamic_cast<Button*>(FindChildByName("ApplyButton"));
	
	// Target cosmetic selection
	m_pCosmeticSearchEntry = dynamic_cast<TextEntry*>(FindChildByName("CosmeticSearchEntry"));
	m_pCosmeticSlotFilterCombo = dynamic_cast<ComboBox*>(FindChildByName("CosmeticSlotFilterCombo"));
	m_pCosmeticCombo = dynamic_cast<ComboBox*>(FindChildByName("CosmeticCombo"));
	m_pBrowseTargetModelButton = dynamic_cast<Button*>(FindChildByName("BrowseTargetModelButton"));
	m_pAddReplacementButton = dynamic_cast<Button*>(FindChildByName("AddReplacementButton"));
	m_pRemoveReplacementButton = dynamic_cast<Button*>(FindChildByName("RemoveReplacementButton"));
	m_pReplacementList = dynamic_cast<ListPanel*>(FindChildByName("ReplacementList"));
	
	// Output directory
	m_pOutputPathEntry = dynamic_cast<TextEntry*>(FindChildByName("OutputPathEntry"));
	m_pBrowseOutputButton = dynamic_cast<Button*>(FindChildByName("BrowseOutputButton"));
	m_pUseTF2FormatCheck = dynamic_cast<CheckButton*>(FindChildByName("UseTF2FormatCheck"));
	
	// Mod info
	m_pModNameEntry = dynamic_cast<TextEntry*>(FindChildByName("ModNameEntry"));
	m_pModDescriptionEntry = dynamic_cast<TextEntry*>(FindChildByName("ModDescriptionEntry"));
	
	// Actions
	m_pGenerateButton = dynamic_cast<Button*>(FindChildByName("GenerateButton"));
	m_pCloseButton = dynamic_cast<Button*>(FindChildByName("CloseButton"));
	m_pStatusLabel = dynamic_cast<Label*>(FindChildByName("StatusLabel"));
	
	// Setup dropdowns
	PopulateClassDropdown();
	
	// Setup team dropdown
	if (m_pTeamCombo)
	{
		m_pTeamCombo->RemoveAll();
		KeyValues* pKV = new KeyValues("data");
		pKV->SetInt("team", TF_TEAM_RED);
		m_pTeamCombo->AddItem("RED", pKV);
		pKV = new KeyValues("data");
		pKV->SetInt("team", TF_TEAM_BLUE);
		m_pTeamCombo->AddItem("BLU", pKV);
		m_pTeamCombo->SilentActivateItemByRow(0);
	}
	
	// Setup cosmetic search entry
	if (m_pCosmeticSearchEntry)
	{
		m_pCosmeticSearchEntry->AddActionSignalTarget(this);
		m_pCosmeticSearchEntry->SendNewLine(true);
	}
	
	// Setup cosmetic slot filter combo
	if (m_pCosmeticSlotFilterCombo)
	{
		m_pCosmeticSlotFilterCombo->AddItem("All Slots", NULL);
		m_pCosmeticSlotFilterCombo->AddItem("Head", NULL);
		m_pCosmeticSlotFilterCombo->AddItem("Misc", NULL);
		m_pCosmeticSlotFilterCombo->AddItem("Action", NULL);
		m_pCosmeticSlotFilterCombo->ActivateItemByRow(0);
		m_pCosmeticSlotFilterCombo->AddActionSignalTarget(this);
	}
	
	// Setup replacement list columns
	if (m_pReplacementList)
	{
		m_pReplacementList->AddColumnHeader(0, "cosmetic", "Target Cosmetic", 200, ListPanel::COLUMN_RESIZEWITHWINDOW);
		m_pReplacementList->AddColumnHeader(1, "model", "Custom Model", 250, ListPanel::COLUMN_RESIZEWITHWINDOW);
	}
	
	// Default states
	if (m_pAutoRotateCheck)
	{
		m_pAutoRotateCheck->SetSelected(false);
	}
	
	// Setup speed slider
	if (m_pAnimSpeedSlider)
	{
		m_pAnimSpeedSlider->SetRange(0, 100);  // 0 = paused, 100 = 10x speed (we'll scale in handler)
		m_pAnimSpeedSlider->SetValue(10);      // Default to 1.0x speed
		m_pAnimSpeedSlider->AddActionSignalTarget(this);
	}
	if (m_pAnimSpeedLabel)
	{
		m_pAnimSpeedLabel->SetText("Speed: 1.0x");
	}
	
	// Need to populate sequence dropdown once player model is fully loaded
	m_bNeedsSequenceDropdownRefresh = true;
	
	// Populate cosmetic dropdown
	PopulateCosmeticDropdown();
	
	SetStatus("Load a cosmetic model to preview");
}

void CCFWorkshopCosmeticTool::PerformLayout()
{
	BaseClass::PerformLayout();
	
	// Ensure preview panel fills its container
	Panel* pPreviewContainer = FindChildByName("CosmeticPreviewPanel");
	if (pPreviewContainer && m_pCosmeticPreview)
	{
		int w, h;
		pPreviewContainer->GetSize(w, h);
		m_pCosmeticPreview->SetSize(w, h);
	}
}

void CCFWorkshopCosmeticTool::OnCommand(const char* command)
{
	if (FStrEq(command, "BrowseModel"))
	{
		OpenModelFileBrowser();
	}
	else if (FStrEq(command, "ResetView"))
	{
		if (m_pCosmeticPreview)
		{
			m_pCosmeticPreview->ResetCamera();
			SetStatus("View reset");
		}
	}
	else if (FStrEq(command, "ToggleAutoRotate"))
	{
		if (m_pAutoRotateCheck && m_pCosmeticPreview)
		{
			m_pCosmeticPreview->SetAutoRotate(m_pAutoRotateCheck->IsSelected());
		}
	}
	else if (FStrEq(command, "ClassChanged"))
	{
		// Just update status - don't auto-apply
		if (m_pClassCombo)
		{
			int iSelected = m_pClassCombo->GetActiveItem();
			if (iSelected >= 0 && iSelected < g_nClassCount)
			{
				SetStatus("Ready to apply: Class set to %s", g_szClassNames[iSelected]);
			}
		}
	}
	else if (FStrEq(command, "TeamChanged"))
	{
		// Just update status - don't auto-apply
		if (m_pTeamCombo)
		{
			KeyValues* pKV = m_pTeamCombo->GetActiveItemUserData();
			if (pKV)
			{
				int iTeam = pKV->GetInt("team", TF_TEAM_RED);
				SetStatus("Ready to apply: Team set to %s", iTeam == TF_TEAM_RED ? "RED" : "BLU");
			}
		}
	}
	else if (FStrEq(command, "ApplySettings"))
	{
		ApplyAllSettings();
	}
	else if (FStrEq(command, "ApplySequence"))
	{
		if (m_pSequenceCombo && m_pCosmeticPreview)
		{
			KeyValues* pKV = m_pSequenceCombo->GetActiveItemUserData();
			if (pKV)
			{
				int iSequence = pKV->GetInt("sequence", 0);
				m_pCosmeticPreview->SetAnimationSequence(iSequence);
				SetStatus("Applied sequence %d", iSequence);
			}
		}
	}
	else if (FStrEq(command, "ToggleBodygroup"))
	{
		ToggleSelectedBodygroup();
	}
	else if (FStrEq(command, "BrowseTargetModel"))
	{
		OpenTargetCosmeticBrowser();
	}
	else if (FStrEq(command, "BrowseOutput"))
	{
		OpenOutputDirectoryBrowser();
	}
	else if (FStrEq(command, "AddReplacement"))
	{
		AddCosmeticReplacement();
	}
	else if (FStrEq(command, "RemoveReplacement"))
	{
		RemoveSelectedReplacement();
	}
	else if (FStrEq(command, "GenerateMod"))
	{
		GenerateCosmeticMod();
	}
	else if (FStrEq(command, "CosmeticSlotFilterChanged"))
	{
		PopulateCosmeticDropdown();
	}
	else if (FStrEq(command, "Close"))
	{
		Close();
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}

void CCFWorkshopCosmeticTool::OnThink()
{
	BaseClass::OnThink();
	
	// Initialize player model once after panel is created and ready
	if (!m_bModelInitialized && m_pCosmeticPreview)
	{
		m_bModelInitialized = true;
		m_pCosmeticPreview->SetToPlayerClass(TF_CLASS_SOLDIER, true);
		m_pCosmeticPreview->SetTeam(TF_TEAM_RED);
	}
	
	// Clear status after timeout
	if (m_flNextStatusClearTime > 0 && gpGlobals->curtime > m_flNextStatusClearTime)
	{
		if (m_pStatusLabel)
		{
			m_pStatusLabel->SetText("");
		}
		m_flNextStatusClearTime = 0;
	}
	
	// Refresh sequence dropdown after model loads
	if (m_bNeedsSequenceDropdownRefresh && m_pCosmeticPreview && m_pCosmeticPreview->GetSequenceCount() > 0)
	{
		m_bNeedsSequenceDropdownRefresh = false;
		PopulateSequenceDropdown();
		PopulateBodygroupList();
	}
}

void CCFWorkshopCosmeticTool::OnKeyCodePressed(KeyCode code)
{
	if (code == KEY_ESCAPE)
	{
		Close();
		return;
	}
	
	BaseClass::OnKeyCodePressed(code);
}

void CCFWorkshopCosmeticTool::ShowPanel(bool bShow)
{
	SetVisible(bShow);
	if (bShow)
	{
		MoveToFront();
		RequestFocus();
	}
}

void CCFWorkshopCosmeticTool::OnFileSelected(KeyValues* params)
{
	const char* pszFullPath = params->GetString("fullpath", "");
	if (!pszFullPath || !pszFullPath[0])
		return;
	
	if (m_eBrowseType == BROWSE_MODEL)
	{
		if (m_pModelPathEntry)
		{
			m_pModelPathEntry->SetText(pszFullPath);
		}
		
		m_strCurrentModelPath = pszFullPath;
		
		if (m_pCosmeticPreview)
		{
			if (m_pCosmeticPreview->LoadCosmeticModel(pszFullPath))
			{
				SetStatus("Loaded: %s", V_GetFileName(pszFullPath));
				
				// Don't auto-apply anymore - wait for Apply button
				// Mark that we need to refresh the sequence dropdown after model loads
				m_bNeedsSequenceDropdownRefresh = true;
			}
			else
			{
				SetStatus("Failed to load model!");
			}
		}
	}
	else if (m_eBrowseType == BROWSE_TARGET_MODEL)
	{
		m_strTargetModelPath = pszFullPath;
		SetStatus("Target cosmetic selected: %s", V_GetFileName(pszFullPath));
	}
}

void CCFWorkshopCosmeticTool::OnDirectorySelected(KeyValues* params)
{
	const char* pszPath = params->GetString("dir");
	if (!pszPath || !pszPath[0])
		return;
	
	m_strOutputPath = pszPath;
	if (m_pOutputPathEntry)
		m_pOutputPathEntry->SetText(pszPath);
	SetStatus("Output directory: %s", pszPath);
}

void CCFWorkshopCosmeticTool::OnSliderMoved(int position)
{
	// Handle animation speed slider
	if (m_pAnimSpeedSlider && m_pCosmeticPreview)
	{
		// Scale: 0 = paused (0.0x), 10 = normal (1.0x), 100 = max (10.0x)
		float flSpeed;
		if (position == 0)
		{
			flSpeed = 0.0f;
		}
		else
		{
			flSpeed = (float)position / 10.0f;  // 10 = 1.0x, 100 = 10.0x
		}
		
		m_pCosmeticPreview->SetAnimationPlaybackRate(flSpeed);
		
		// Update the label
		if (m_pAnimSpeedLabel)
		{
			char szText[64];
			if (flSpeed == 0.0f)
			{
				Q_snprintf(szText, sizeof(szText), "Speed: Paused");
			}
			else
			{
				Q_snprintf(szText, sizeof(szText), "Speed: %.1fx", flSpeed);
			}
			m_pAnimSpeedLabel->SetText(szText);
		}
	}
}

void CCFWorkshopCosmeticTool::OnTextChanged(KeyValues* params)
{
	Panel* pPanel = (Panel*)params->GetPtr("panel");
	if (pPanel == m_pCosmeticSearchEntry)
	{
		PopulateCosmeticDropdown();
	}
}

void CCFWorkshopCosmeticTool::OpenModelFileBrowser()
{
	m_eBrowseType = BROWSE_MODEL;
	FileOpenDialog* pDialog = new FileOpenDialog(this, "Select Cosmetic Model", true);
	pDialog->AddFilter("*.mdl", "Model Files (*.mdl)", true);
	pDialog->AddFilter("*.*", "All Files (*.*)", false);
	
	char szGameDir[MAX_PATH];
	g_pFullFileSystem->GetCurrentDirectory(szGameDir, sizeof(szGameDir));
	Q_strncat(szGameDir, "\\models", sizeof(szGameDir), COPY_ALL_CHARACTERS);
	
	pDialog->SetStartDirectory(szGameDir);
	pDialog->DoModal(false);
}

void CCFWorkshopCosmeticTool::UpdatePreview()
{
	if (m_pCosmeticPreview && !m_strCurrentModelPath.IsEmpty())
	{
		m_pCosmeticPreview->LoadCosmeticModel(m_strCurrentModelPath.Get());
	}
}

void CCFWorkshopCosmeticTool::PopulateClassDropdown()
{
	if (!m_pClassCombo)
		return;
	
	m_pClassCombo->RemoveAll();
	
	for (int i = 0; i < g_nClassCount; i++)
	{
		KeyValues* pKV = new KeyValues("data");
		pKV->SetInt("class", g_ClassIndices[i]);
		m_pClassCombo->AddItem(g_szClassNames[i], pKV);
	}
	
	// Default to Soldier (index 1)
	m_pClassCombo->SilentActivateItemByRow(1);
}

void CCFWorkshopCosmeticTool::PopulateSequenceDropdown()
{
	if (!m_pSequenceCombo || !m_pCosmeticPreview)
		return;
	
	m_pSequenceCombo->RemoveAll();
	
	int nSequences = m_pCosmeticPreview->GetSequenceCount();
	for (int i = 0; i < nSequences; i++)
	{
		const char* pszName = m_pCosmeticPreview->GetSequenceName(i);
		if (pszName && pszName[0])
		{
			KeyValues* pKV = new KeyValues("data");
			pKV->SetInt("sequence", i);
			
			char szLabel[256];
			Q_snprintf(szLabel, sizeof(szLabel), "%d: %s", i, pszName);
			m_pSequenceCombo->AddItem(szLabel, pKV);
		}
		else
		{
			KeyValues* pKV = new KeyValues("data");
			pKV->SetInt("sequence", i);
			
			char szLabel[64];
			Q_snprintf(szLabel, sizeof(szLabel), "%d: <unnamed>", i);
			m_pSequenceCombo->AddItem(szLabel, pKV);
		}
	}
	
	if (nSequences > 0)
	{
		m_pSequenceCombo->SilentActivateItemByRow(0);
	}
}

void CCFWorkshopCosmeticTool::PopulateBodygroupList()
{
	if (!m_pBodygroupList || !m_pCosmeticPreview)
		return;
	
	m_pBodygroupList->RemoveAll();
	
	int nBodygroups = m_pCosmeticPreview->GetBodygroupCount();
	for (int i = 0; i < nBodygroups; i++)
	{
		const char* pszName = m_pCosmeticPreview->GetBodygroupName(i);
		int iValue = m_pCosmeticPreview->GetBodygroupValue(i);
		int iNumModels = m_pCosmeticPreview->GetBodygroupNumModels(i);
		
		if (pszName && pszName[0])
		{
			KeyValues* pKV = new KeyValues("data");
			pKV->SetInt("index", i);
			pKV->SetInt("nummodels", iNumModels);
			pKV->SetString("name", pszName);
			pKV->SetString("bodygroup", pszName);
			
			char szValue[32];
			Q_snprintf(szValue, sizeof(szValue), "%d/%d", iValue, iNumModels - 1);
			pKV->SetString("value", szValue);
			
			m_pBodygroupList->AddItem(pKV, 0, false, false);
			pKV->deleteThis();
		}
	}
	
	m_pBodygroupList->SortList();
}

void CCFWorkshopCosmeticTool::ToggleSelectedBodygroup()
{
	if (!m_pBodygroupList || !m_pCosmeticPreview)
		return;
	
	int iSelected = m_pBodygroupList->GetSelectedItem(0);
	if (iSelected == -1)
	{
		SetStatus("Please select a bodygroup to toggle");
		return;
	}
	
	KeyValues* pKV = m_pBodygroupList->GetItem(iSelected);
	if (!pKV)
		return;
	
	int iBodygroup = pKV->GetInt("index", -1);
	int iNumModels = pKV->GetInt("nummodels", 1);
	
	if (iBodygroup < 0)
		return;
	
	// Toggle to next value (wrapping around)
	int iCurrentValue = m_pCosmeticPreview->GetBodygroupValue(iBodygroup);
	int iNewValue = (iCurrentValue + 1) % iNumModels;
	
	m_pCosmeticPreview->SetBodygroupValue(iBodygroup, iNewValue);
	
	SetStatus("Bodygroup '%s' set to %d", pKV->GetString("name", ""), iNewValue);
	
	// Refresh the list to show updated value
	PopulateBodygroupList();
}

void CCFWorkshopCosmeticTool::ApplyAllSettings()
{
	if (!m_pCosmeticPreview)
		return;
	
	// Apply class change
	if (m_pClassCombo)
	{
		int iSelected = m_pClassCombo->GetActiveItem();
		if (iSelected >= 0 && iSelected < g_nClassCount)
		{
			int iNewClass = g_ClassIndices[iSelected];
			if (iNewClass != m_pCosmeticPreview->GetPlayerClass())
			{
				m_pCosmeticPreview->SetPreviewClass(iNewClass);
				m_bNeedsSequenceDropdownRefresh = true;
			}
		}
	}
	
	// Apply team change
	if (m_pTeamCombo)
	{
		KeyValues* pKV = m_pTeamCombo->GetActiveItemUserData();
		if (pKV)
		{
			int iTeam = pKV->GetInt("team", TF_TEAM_RED);
			if (iTeam != m_pCosmeticPreview->GetTeam())
			{
				m_pCosmeticPreview->SetPreviewTeam(iTeam);
			}
		}
	}
	
	SetStatus("Settings applied!");
}

void CCFWorkshopCosmeticTool::PopulateCosmeticDropdown()
{
	if (!m_pCosmeticCombo)
		return;
	
	m_pCosmeticCombo->RemoveAll();
	
	int slotFilter = m_pCosmeticSlotFilterCombo ? m_pCosmeticSlotFilterCombo->GetActiveItem() : 0;
	
	// Get search text
	char szSearch[256] = {0};
	if (m_pCosmeticSearchEntry)
	{
		m_pCosmeticSearchEntry->GetText(szSearch, sizeof(szSearch));
	}
	
	// Get all item definitions from the schema
	const CEconItemSchema::ItemDefinitionMap_t& mapItemDefs = ItemSystem()->GetItemSchema()->GetItemDefinitionMap();
	
	// Track seen cosmetic names to filter duplicates
	CUtlStringMap<bool> mapSeenNames;
	
	FOR_EACH_MAP_FAST(mapItemDefs, i)
	{
		CEconItemDefinition* pBaseDef = mapItemDefs[i];
		if (!pBaseDef)
			continue;
		
		const CTFItemDefinition* pItemDef = dynamic_cast<const CTFItemDefinition*>(pBaseDef);
		if (!pItemDef)
			continue;
		
		// Skip hidden items
		if (pItemDef->IsHidden())
			continue;
		
		// Get the loadout slot
		int iSlot = pItemDef->GetLoadoutSlot(TF_CLASS_SCOUT);
		
		// Only include cosmetic slots (head, misc, action)
		if (iSlot != LOADOUT_POSITION_HEAD && 
			iSlot != LOADOUT_POSITION_MISC && 
			iSlot != LOADOUT_POSITION_MISC2 &&
			iSlot != LOADOUT_POSITION_ACTION)
			continue;
		
		// Apply slot filter
		if (slotFilter > 0)
		{
			int iWantedSlot = -1;
			switch (slotFilter)
			{
				case 1: iWantedSlot = LOADOUT_POSITION_HEAD; break;
				case 2: iWantedSlot = LOADOUT_POSITION_MISC; break;
				case 3: iWantedSlot = LOADOUT_POSITION_ACTION; break;
			}
			
			if (iWantedSlot != -1 && iSlot != iWantedSlot && 
				!(iWantedSlot == LOADOUT_POSITION_MISC && iSlot == LOADOUT_POSITION_MISC2))
				continue;
		}
		
		// Get the model path
		const char* pszModel = pItemDef->GetBasePlayerDisplayModel();
		if (!pszModel || !pszModel[0])
			pszModel = pItemDef->GetWorldDisplayModel();
		
		if (!pszModel || !pszModel[0])
			continue;
		
		// Get display name
		const char* pszBaseName = pItemDef->GetItemBaseName();
		const char* pszDisplayName = pItemDef->GetDefinitionName();
		
		if (pszBaseName && pszBaseName[0] == '#')
		{
			const wchar_t* pwszLocalized = g_pVGuiLocalize->Find(pszBaseName);
			if (pwszLocalized)
			{
				static char szLocalizedName[256];
				g_pVGuiLocalize->ConvertUnicodeToANSI(pwszLocalized, szLocalizedName, sizeof(szLocalizedName));
				pszDisplayName = szLocalizedName;
			}
		}
		
		// Skip duplicates
		if (mapSeenNames.Defined(pszDisplayName))
			continue;
		mapSeenNames[pszDisplayName] = true;
		
		// Apply search filter
		if (szSearch[0] != '\0')
		{
			if (!Q_stristr(pszDisplayName, szSearch))
				continue;
		}
		
		KeyValues* pData = new KeyValues("data");
		pData->SetString("model", pszModel);
		m_pCosmeticCombo->AddItem(pszDisplayName, pData);
	}
	
	if (m_pCosmeticCombo->GetItemCount() > 0)
		m_pCosmeticCombo->ActivateItemByRow(0);
}

void CCFWorkshopCosmeticTool::FilterCosmeticsBySearch()
{
	PopulateCosmeticDropdown();
}

void CCFWorkshopCosmeticTool::OpenTargetCosmeticBrowser()
{
	m_eBrowseType = BROWSE_TARGET_MODEL;
	FileOpenDialog* pDialog = new FileOpenDialog(this, "Select Target Cosmetic Model", true);
	pDialog->AddFilter("*.mdl", "Model Files (*.mdl)", true);
	pDialog->AddFilter("*.*", "All Files (*.*)", false);
	
	char szGameDir[MAX_PATH];
	g_pFullFileSystem->GetCurrentDirectory(szGameDir, sizeof(szGameDir));
	Q_strncat(szGameDir, "\\models", sizeof(szGameDir), COPY_ALL_CHARACTERS);
	
	pDialog->SetStartDirectory(szGameDir);
	pDialog->DoModal(false);
}

void CCFWorkshopCosmeticTool::OpenOutputDirectoryBrowser()
{
	m_eBrowseType = BROWSE_OUTPUT;
	DirectorySelectDialog* pDialog = new DirectorySelectDialog(this, "Select Output Directory");
	pDialog->AddActionSignalTarget(this);
	
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

void CCFWorkshopCosmeticTool::AddCosmeticReplacement()
{
	if (m_strCurrentModelPath.IsEmpty())
	{
		SetStatus("Please select a custom model file first!");
		return;
	}
	
	// Get target model path
	CUtlString strTargetPath;
	CUtlString strDisplayName;
	
	// Check if using dropdown selection or manual path
	if (m_pCosmeticCombo && m_pCosmeticCombo->GetActiveItem() >= 0)
	{
		KeyValues* pData = m_pCosmeticCombo->GetActiveItemUserData();
		if (pData)
		{
			strTargetPath = pData->GetString("model", "");
			
			// Get display name from combo
			char szText[256];
			m_pCosmeticCombo->GetItemText(m_pCosmeticCombo->GetActiveItem(), szText, sizeof(szText));
			strDisplayName = szText;
		}
	}
	else if (!m_strTargetModelPath.IsEmpty())
	{
		strTargetPath = m_strTargetModelPath;
		strDisplayName = V_GetFileName(m_strTargetModelPath.Get());
	}
	
	if (strTargetPath.IsEmpty())
	{
		SetStatus("Please select a target cosmetic!");
		return;
	}
	
	// Add to our data
	CosmeticReplacementEntry_t entry;
	entry.strCustomModelPath = m_strCurrentModelPath;
	entry.strTargetModelPath = strTargetPath;
	entry.strDisplayName = strDisplayName;
	m_vecReplacements.AddToTail(entry);
	
	// Add to list panel
	if (m_pReplacementList)
	{
		KeyValues* pKV = new KeyValues("item");
		pKV->SetString("cosmetic", strDisplayName.Get());
		pKV->SetString("model", V_GetFileName(m_strCurrentModelPath.Get()));
		m_pReplacementList->AddItem(pKV, 0, false, false);
		pKV->deleteThis();
	}
	
	SetStatus("Added replacement: %s -> %s", strDisplayName.Get(), V_GetFileName(m_strCurrentModelPath.Get()));
}

void CCFWorkshopCosmeticTool::RemoveSelectedReplacement()
{
	if (!m_pReplacementList)
		return;
	
	int sel = m_pReplacementList->GetSelectedItem(0);
	if (sel < 0)
		return;
	
	int dataIdx = m_pReplacementList->GetItemCurrentRow(sel);
	if (dataIdx >= 0 && dataIdx < m_vecReplacements.Count())
		m_vecReplacements.Remove(dataIdx);
	
	m_pReplacementList->RemoveItem(sel);
	SetStatus("Replacement removed");
}

void CCFWorkshopCosmeticTool::GenerateCosmeticMod()
{
	if (m_vecReplacements.Count() == 0)
	{
		SetStatus("No cosmetic replacements defined!");
		return;
	}
	
	if (m_strOutputPath.IsEmpty())
	{
		SetStatus("Please select an output directory!");
		return;
	}
	
	bool bUseTF2Format = m_pUseTF2FormatCheck && m_pUseTF2FormatCheck->IsSelected();
	
	int nSuccessCount = 0;
	
	// Process each replacement
	for (int i = 0; i < m_vecReplacements.Count(); i++)
	{
		const CosmeticReplacementEntry_t& entry = m_vecReplacements[i];
		if (entry.strTargetModelPath.IsEmpty())
			continue;
		
		// Copy to the target model path
		if (CopyModelToTarget(entry.strCustomModelPath.Get(), entry.strTargetModelPath.Get()))
			nSuccessCount++;
	}
	
	if (nSuccessCount > 0)
	{
		if (bUseTF2Format)
		{
			// Generate TF2 manifest and zip
			if (GenerateTF2Manifest())
			{
				SetStatus("Created TF2 import package at: %s", m_strOutputPath.Get());
				
				char szMsg[512];
				V_snprintf(szMsg, sizeof(szMsg), "Successfully created TF2 cosmetic import package!\n\nOutput: %s\\cosmetic_mod.zip", m_strOutputPath.Get());
				MessageBox* pMsg = new MessageBox("Success", szMsg, this);
				pMsg->DoModal();
			}
			else
			{
				SetStatus("Failed to create TF2 package!");
			}
		}
		else
		{
			SetStatus("Created %d model replacement(s) at: %s", nSuccessCount, m_strOutputPath.Get());
			
			// Show success message
			char szMsg[512];
			V_snprintf(szMsg, sizeof(szMsg), "Successfully created %d cosmetic mod file(s)!\n\nOutput: %s", nSuccessCount, m_strOutputPath.Get());
			MessageBox* pMsg = new MessageBox("Success", szMsg, this);
			pMsg->DoModal();
		}
	}
	else
	{
		SetStatus("Failed to copy model files!");
	}
}

bool CCFWorkshopCosmeticTool::CopyModelToTarget(const char* pszSourcePath, const char* pszTargetModelPath)
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
		char szGameDir[MAX_PATH];
		g_pFullFileSystem->GetCurrentDirectory(szGameDir, sizeof(szGameDir));
		V_snprintf(szSourceFullBase, sizeof(szSourceFullBase), "%s%c%s", szGameDir, CORRECT_PATH_SEPARATOR, szSourceBase);
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
		char szSourceFile[MAX_PATH];
		char szDestFile[MAX_PATH];
		
		V_snprintf(szSourceFile, sizeof(szSourceFile), "%s%s", szSourceFullBase, szExtensions[i]);
		V_snprintf(szDestFile, sizeof(szDestFile), "%s%s", szDestBase, szExtensions[i]);
		
		if (!g_pFullFileSystem->FileExists(szSourceFile))
			continue;
		
		// Read source file
		FileHandle_t hSource = g_pFullFileSystem->Open(szSourceFile, "rb");
		if (hSource == FILESYSTEM_INVALID_HANDLE)
			continue;
		
		int nSize = g_pFullFileSystem->Size(hSource);
		byte* pBuffer = new byte[nSize];
		g_pFullFileSystem->Read(pBuffer, nSize, hSource);
		g_pFullFileSystem->Close(hSource);
		
		// Write dest file (use stdio for writing outside game dir)
		FILE* pDestFile = fopen(szDestFile, "wb");
		if (pDestFile)
		{
			fwrite(pBuffer, 1, nSize, pDestFile);
			fclose(pDestFile);
			nFilesCopied++;
		}
		
		delete[] pBuffer;
	}
	
	return nFilesCopied > 0;
}

bool CCFWorkshopCosmeticTool::GenerateTF2Manifest()
{
	if (m_strOutputPath.IsEmpty() || m_vecReplacements.Count() == 0)
		return false;
	
	// Create a simple README for TF2 usage
	char szReadmePath[MAX_PATH];
	V_snprintf(szReadmePath, sizeof(szReadmePath), "%s%cREADME.txt", m_strOutputPath.Get(), CORRECT_PATH_SEPARATOR);
	
	FILE* pReadme = fopen(szReadmePath, "w");
	if (pReadme)
	{
		fprintf(pReadme, "TF2 Custom Cosmetic Replacement Mod\n");
		fprintf(pReadme, "===================================\n\n");
		fprintf(pReadme, "Installation:\n");
		fprintf(pReadme, "1. Extract cosmetic_mod.zip to your tf/custom/ folder\n");
		fprintf(pReadme, "2. Launch TF2\n\n");
		fprintf(pReadme, "This mod replaces the following cosmetics:\n\n");
		
		for (int i = 0; i < m_vecReplacements.Count(); i++)
		{
			const CosmeticReplacementEntry_t& entry = m_vecReplacements[i];
			if (entry.strTargetModelPath.IsEmpty())
				continue;
			
			fprintf(pReadme, "  - %s -> Custom model\n", entry.strDisplayName.Get());
		}
		
		fprintf(pReadme, "\nModel Files Replaced:\n\n");
		
		for (int i = 0; i < m_vecReplacements.Count(); i++)
		{
			const CosmeticReplacementEntry_t& entry = m_vecReplacements[i];
			if (entry.strTargetModelPath.IsEmpty())
				continue;
			
			char szTargetPath[MAX_PATH];
			V_strcpy_safe(szTargetPath, entry.strTargetModelPath.Get());
			V_FixSlashes(szTargetPath, '/');
			fprintf(pReadme, "  %s\n", szTargetPath);
		}
		
		fclose(pReadme);
	}
	
	// Create zip file (don't include .zip extension, PowerShell adds it)
	char szZipPath[MAX_PATH];
	V_snprintf(szZipPath, sizeof(szZipPath), "%s%ccosmetic_mod", m_strOutputPath.Get(), CORRECT_PATH_SEPARATOR);
	
	return CreateZipFile(szZipPath, m_strOutputPath.Get());
}

bool CCFWorkshopCosmeticTool::CreateZipFile(const char* pszZipPath, const char* pszSourceDir)
{
#ifdef _WIN32
	// Use PowerShell Compress-Archive on Windows
	// PowerShell adds .zip automatically, so pszZipPath should not include it
	char szCommand[2048];
	V_snprintf(szCommand, sizeof(szCommand), 
		"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"Compress-Archive -Path \\\"%s\\*\\\" -DestinationPath \\\"%s\\\" -Force\"",
		pszSourceDir, pszZipPath);
	
	// Call system function from global namespace (C runtime)
	typedef int (*SystemFunc)(const char*);
	SystemFunc pSystemFunc = &std::system;
	int result = pSystemFunc(szCommand);
	return result == 0;
#else
	// Use zip command on Linux/Mac
	char szCommand[1024];
	char szOldDir[MAX_PATH];
	getcwd(szOldDir, sizeof(szOldDir));
	
	chdir(pszSourceDir);
	V_snprintf(szCommand, sizeof(szCommand), "zip -r '%s' .", pszZipPath);
	typedef int (*SystemFunc)(const char*);
	SystemFunc pSystemFunc = &std::system;
	int result = pSystemFunc(szCommand);
	chdir(szOldDir);
	
	return result == 0;
#endif
}

void CCFWorkshopCosmeticTool::SetStatus(const char* pszFormat, ...)
{
	if (!m_pStatusLabel)
		return;
	
	char szBuffer[512];
	va_list args;
	va_start(args, pszFormat);
	V_vsnprintf(szBuffer, sizeof(szBuffer), pszFormat, args);
	va_end(args);
	
	m_pStatusLabel->SetText(szBuffer);
	m_flNextStatusClearTime = gpGlobals->curtime + 5.0f;
}

//-----------------------------------------------------------------------------
// Console command to open the tool
//-----------------------------------------------------------------------------
void OpenWorkshopCosmeticTool()
{
	CCFWorkshopCosmeticTool* pTool = new CCFWorkshopCosmeticTool(NULL, "WorkshopCosmeticTool");
	pTool->ShowPanel(true);
}

CON_COMMAND(cf_workshop_cosmetic_tool, "Open the Workshop Cosmetic Preview Tool")
{
	OpenWorkshopCosmeticTool();
}

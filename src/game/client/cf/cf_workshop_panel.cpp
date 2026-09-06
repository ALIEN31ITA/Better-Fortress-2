//====== Copyright Custom Fortress 2, All rights reserved. =================
//
// Workshop Browser UI Panel Implementation
//
//=============================================================================

#include "cbase.h"
#include "../cf/cf_workshop_panel.h"
#include "../cf/cf_workshop_manager.h"
#include <vgui/IVGui.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/IInput.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Menu.h>
#include <vgui_controls/FileOpenDialog.h>
#include "vgui/ILocalize.h"
#include "ienginevgui.h"
#include "imageutils.h"
#include "bitmap/bitmap.h"
#include "utlbuffer.h"
#include "filesystem.h"
#include "cdll_int.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// CWorkshopPreviewImage - Simple IImage wrapper for workshop preview textures
//-----------------------------------------------------------------------------
CWorkshopPreviewImage::CWorkshopPreviewImage()
	: m_nTextureID(-1)
	, m_nX(0)
	, m_nY(0)
	, m_nWide(0)
	, m_nTall(0)
	, m_nImageWidth(0)
	, m_nImageHeight(0)
	, m_bValid(false)
{
	m_Color = Color(255, 255, 255, 255);
}

CWorkshopPreviewImage::~CWorkshopPreviewImage()
{
	Clear();
}

void CWorkshopPreviewImage::SetTextureRGBA(const byte* rgba, int width, int height)
{
	if (!rgba || width <= 0 || height <= 0)
		return;
	
	if (m_nTextureID == -1)
	{
		m_nTextureID = surface()->CreateNewTextureID(true);
	}
	
	surface()->DrawSetTextureRGBAEx(m_nTextureID, rgba, width, height, IMAGE_FORMAT_RGBA8888);
	
	m_nImageWidth = width;
	m_nImageHeight = height;
	
	// If size not explicitly set, use image size
	if (m_nWide == 0)
		m_nWide = width;
	if (m_nTall == 0)
		m_nTall = height;
	
	m_bValid = true;
}

void CWorkshopPreviewImage::Clear()
{
	if (m_nTextureID != -1)
	{
		surface()->DestroyTextureID(m_nTextureID);
		m_nTextureID = -1;
	}
	m_bValid = false;
	m_nImageWidth = 0;
	m_nImageHeight = 0;
}

bool CWorkshopPreviewImage::SetTextureFromFile(const char* pszFilePath)
{
	if (!pszFilePath || !pszFilePath[0])
		return false;
	
	// Load the image using imageutils
	int width = 0;
	int height = 0;
	ConversionErrorType errcode = CE_SUCCESS;
	unsigned char* pRGBA = NULL;
	
	// Determine format from extension
	const char* pszExt = V_GetFileExtension(pszFilePath);
	
	if (pszExt && (V_stricmp(pszExt, "jpg") == 0 || V_stricmp(pszExt, "jpeg") == 0))
	{
		pRGBA = ImgUtl_ReadJPEGAsRGBA(pszFilePath, width, height, errcode);
	}
	else if (pszExt && V_stricmp(pszExt, "png") == 0)
	{
		pRGBA = ImgUtl_ReadPNGAsRGBA(pszFilePath, width, height, errcode);
	}
	else
	{
		Warning("Unsupported image format: %s\n", pszExt ? pszExt : "unknown");
		return false;
	}
	
	if (!pRGBA || errcode != CE_SUCCESS)
	{
		Warning("Failed to load image: %s (error code: %d)\n", pszFilePath, errcode);
		if (pRGBA)
			delete[] pRGBA;
		return false;
	}
	
	// Set the texture from RGBA data
	SetTextureRGBA(pRGBA, width, height);
	
	// Free the temporary buffer
	delete[] pRGBA;
	
	return true;
}

void CWorkshopPreviewImage::Paint()
{
	if (!m_bValid || m_nTextureID == -1)
		return;
	
	surface()->DrawSetTexture(m_nTextureID);
	surface()->DrawSetColor(m_Color);
	surface()->DrawTexturedRect(m_nX, m_nY, m_nX + m_nWide, m_nY + m_nTall);
}

//-----------------------------------------------------------------------------
// Workshop Item Details Panel
//-----------------------------------------------------------------------------
CCFWorkshopItemDetails::CCFWorkshopItemDetails(Panel* parent, const char* panelName)
	: BaseClass(parent, panelName)
	, m_pCurrentItem(NULL)
	, m_pContainer(NULL)
	, m_bPendingSubscribe(false)
	, m_bPendingUnsubscribe(false)
	, m_hPendingPreviewRequest(INVALID_HTTPREQUEST_HANDLE)
	, m_pPreviewImageData(NULL)
	, m_nPreviewFileID(0)
	, m_pTitleLabel(NULL)
	, m_pAuthorLabel(NULL)
	, m_pDescriptionLabel(NULL)
	, m_pFileSizeLabel(NULL)
	, m_pUpdatedLabel(NULL)
	, m_pTagsLabel(NULL)
	, m_pPreviewImage(NULL)
	, m_pSubscribeButton(NULL)
	, m_pUnsubscribeButton(NULL)
	, m_pOpenWorkshopButton(NULL)
	, m_pEnableCheckbox(NULL)
{
	// Create our preview image object
	m_pPreviewImageData = new CWorkshopPreviewImage();
}

void CCFWorkshopItemDetails::SetParentContainer(EditablePanel* pContainer)
{
	m_pContainer = pContainer;
	if (!pContainer)
		return;
	
	// Find controls from the container (created by .res file)
	m_pTitleLabel = dynamic_cast<Label*>(pContainer->FindChildByName("TitleLabel"));
	m_pAuthorLabel = dynamic_cast<Label*>(pContainer->FindChildByName("AuthorLabel"));
	m_pDescriptionLabel = dynamic_cast<Label*>(pContainer->FindChildByName("DescriptionLabel"));
	m_pFileSizeLabel = dynamic_cast<Label*>(pContainer->FindChildByName("FileSizeLabel"));
	m_pTagsLabel = dynamic_cast<Label*>(pContainer->FindChildByName("TagsLabel"));
	m_pPreviewImage = dynamic_cast<ImagePanel*>(pContainer->FindChildByName("PreviewImage"));
	m_pSubscribeButton = dynamic_cast<Button*>(pContainer->FindChildByName("SubscribeButton"));
	m_pUnsubscribeButton = dynamic_cast<Button*>(pContainer->FindChildByName("UnsubscribeButton"));
	m_pOpenWorkshopButton = dynamic_cast<Button*>(pContainer->FindChildByName("OpenWorkshopButton"));
	
	// Make buttons send commands to us
	if (m_pSubscribeButton)
	{
		m_pSubscribeButton->SetCommand("subscribe");
		m_pSubscribeButton->AddActionSignalTarget(this);
	}
	if (m_pUnsubscribeButton)
	{
		m_pUnsubscribeButton->SetCommand("unsubscribe");
		m_pUnsubscribeButton->AddActionSignalTarget(this);
	}
	if (m_pOpenWorkshopButton)
	{
		m_pOpenWorkshopButton->SetCommand("openworkshop");
		m_pOpenWorkshopButton->AddActionSignalTarget(this);
	}
	
	// Find enable/disable checkbox from .res, or create it dynamically
	m_pEnableCheckbox = dynamic_cast<CheckButton*>(pContainer->FindChildByName("EnableCheckbox"));
	if (!m_pEnableCheckbox)
	{
		m_pEnableCheckbox = new CheckButton(pContainer, "EnableCheckbox", "Enabled");
		m_pEnableCheckbox->SetPos(10, 485);
		m_pEnableCheckbox->SetSize(135, 20);
	}
	if (m_pEnableCheckbox)
	{
		m_pEnableCheckbox->SetCommand("toggle_enable");
		m_pEnableCheckbox->AddActionSignalTarget(this);
	}
	
	// Initially hide the action buttons
	Clear();
}

CCFWorkshopItemDetails::~CCFWorkshopItemDetails()
{
	// Cancel any pending HTTP request
	if (m_hPendingPreviewRequest != INVALID_HTTPREQUEST_HANDLE)
	{
		ISteamHTTP* pHTTP = SteamHTTP();
		if (pHTTP)
		{
			pHTTP->ReleaseHTTPRequest(m_hPendingPreviewRequest);
		}
		m_hPendingPreviewRequest = INVALID_HTTPREQUEST_HANDLE;
	}
	
	// Clean up preview image
	if (m_pPreviewImageData)
	{
		delete m_pPreviewImageData;
		m_pPreviewImageData = NULL;
	}
}

void CCFWorkshopItemDetails::SetItem(CCFWorkshopItem* pItem)
{
	m_pCurrentItem = pItem;
	m_bPendingSubscribe = false;
	m_bPendingUnsubscribe = false;
	
	if (!pItem)
	{
		Clear();
		return;
	}

	// Update labels
	if (m_pTitleLabel) m_pTitleLabel->SetText(pItem->GetTitle());
	if (m_pDescriptionLabel) m_pDescriptionLabel->SetText(pItem->GetDescription());
	if (m_pTagsLabel) 
	{
		char szTags[512];
		V_snprintf(szTags, sizeof(szTags), "Tags: %s", pItem->GetTags());
		m_pTagsLabel->SetText(szTags);
	}
	
	// Format file size
	if (m_pFileSizeLabel)
	{
		float sizeMB = pItem->GetFileSize() / (1024.0f * 1024.0f);
		char szSize[64];
		V_snprintf(szSize, sizeof(szSize), "Size: %.2f MB", sizeMB);
		m_pFileSizeLabel->SetText(szSize);
	}
	
	// Request preview image from Steam if URL is available
	if (m_pPreviewImage)
	{
		const char* pszPreviewURL = pItem->GetPreviewURL();
		CFWorkshopMsg("Preview URL for item %llu: %s\n", pItem->GetFileID(), pszPreviewURL ? pszPreviewURL : "(null)");
		if (pszPreviewURL && pszPreviewURL[0])
		{
			// Request the preview image via HTTP
			RequestPreviewImage(pszPreviewURL);
			m_nPreviewFileID = pItem->GetFileID();
		}
		else
		{
			// Set a placeholder while we wait or if no URL
			m_pPreviewImage->SetImage("vgui/maps/menu_thumb_missing");
		}
		m_pPreviewImage->SetVisible(true);
	}
	
	// Update button states
	RefreshButtonStates();
}

void CCFWorkshopItemDetails::Clear()
{
	m_pCurrentItem = NULL;
	m_bPendingSubscribe = false;
	m_bPendingUnsubscribe = false;
	m_nPreviewFileID = 0;
	vgui::ivgui()->RemoveTickSignal(GetVPanel());
	
	// Cancel any pending HTTP request
	if (m_hPendingPreviewRequest != INVALID_HTTPREQUEST_HANDLE)
	{
		ISteamHTTP* pHTTP = SteamHTTP();
		if (pHTTP)
		{
			pHTTP->ReleaseHTTPRequest(m_hPendingPreviewRequest);
		}
		m_hPendingPreviewRequest = INVALID_HTTPREQUEST_HANDLE;
	}
	
	// Clear the preview image data
	if (m_pPreviewImageData)
	{
		m_pPreviewImageData->Clear();
	}
	
	if (m_pTitleLabel) m_pTitleLabel->SetText("");
	if (m_pDescriptionLabel) m_pDescriptionLabel->SetText("");
	if (m_pTagsLabel) m_pTagsLabel->SetText("");
	if (m_pFileSizeLabel) m_pFileSizeLabel->SetText("");
	if (m_pUpdatedLabel) m_pUpdatedLabel->SetText("");
	if (m_pPreviewImage)
	{
		m_pPreviewImage->SetImage((vgui::IImage*)NULL);
		m_pPreviewImage->SetVisible(false);
	}
	if (m_pSubscribeButton)
	{
		m_pSubscribeButton->SetText("Subscribe");
		m_pSubscribeButton->SetVisible(false);
	}
	if (m_pUnsubscribeButton)
	{
		m_pUnsubscribeButton->SetText("Unsubscribe");
		m_pUnsubscribeButton->SetVisible(false);
	}
	if (m_pOpenWorkshopButton) m_pOpenWorkshopButton->SetVisible(false);
	if (m_pEnableCheckbox)
	{
		m_pEnableCheckbox->SetSelected(false);
		m_pEnableCheckbox->SetVisible(false);
	}
}

void CCFWorkshopItemDetails::ApplySchemeSettings(IScheme* pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
}

void CCFWorkshopItemDetails::OnCommand(const char* command)
{
	if (!m_pCurrentItem || !CFWorkshop())
	{
		return;
	}

	if (Q_stricmp(command, "subscribe") == 0)
	{
		CFWorkshop()->SubscribeItem(m_pCurrentItem->GetFileID());
		m_bPendingSubscribe = true;
		// Show "Subscribing..." on the button
		if (m_pSubscribeButton)
			m_pSubscribeButton->SetText("Subscribing...");
		// Start tick to check for completion
		vgui::ivgui()->AddTickSignal(GetVPanel(), 500);
	}
	else if (Q_stricmp(command, "unsubscribe") == 0)
	{
		CFWorkshop()->UnsubscribeItem(m_pCurrentItem->GetFileID());
		m_bPendingUnsubscribe = true;
		// Show "Unsubscribing..." on the button
		if (m_pUnsubscribeButton)
			m_pUnsubscribeButton->SetText("Unsubscribing...");
		// Start tick to check for completion
		vgui::ivgui()->AddTickSignal(GetVPanel(), 500);
	}
	else if (Q_stricmp(command, "openworkshop") == 0)
	{
		// Open in Steam Workshop overlay
		if (steamapicontext && steamapicontext->SteamFriends())
		{
			char szURL[256];
			V_snprintf(szURL, sizeof(szURL), "https://steamcommunity.com/sharedfiles/filedetails/?id=%llu", 
				m_pCurrentItem->GetFileID());
			steamapicontext->SteamFriends()->ActivateGameOverlayToWebPage(szURL);
		}
	}
	else if (Q_stricmp(command, "toggle_enable") == 0)
	{
		// Toggle enable/disable state
		if (m_pEnableCheckbox)
		{
			bool bEnabled = m_pEnableCheckbox->IsSelected();
			CFWorkshop()->SetItemEnabled(m_pCurrentItem->GetFileID(), bEnabled);
			
			// Show feedback
			if (bEnabled)
				Msg("[CF Workshop] Mod '%s' enabled.\n", m_pCurrentItem->GetTitle());
			else
				Msg("[CF Workshop] Mod '%s' disabled.\n", m_pCurrentItem->GetTitle());
		}
	}
}

void CCFWorkshopItemDetails::OnTick()
{
	if (!m_pCurrentItem)
	{
		vgui::ivgui()->RemoveTickSignal(GetVPanel());
		return;
	}

	// Check if pending subscribe/unsubscribe has completed
	bool bSubscribed = m_pCurrentItem->IsSubscribed();
	
	if (m_bPendingSubscribe && bSubscribed)
	{
		// Subscribe completed
		m_bPendingSubscribe = false;
		RefreshButtonStates();
		vgui::ivgui()->RemoveTickSignal(GetVPanel());
	}
	else if (m_bPendingUnsubscribe && !bSubscribed)
	{
		// Unsubscribe completed
		m_bPendingUnsubscribe = false;
		RefreshButtonStates();
		vgui::ivgui()->RemoveTickSignal(GetVPanel());
	}
}

void CCFWorkshopItemDetails::RefreshButtonStates()
{
	if (!m_pCurrentItem)
		return;
		
	bool bSubscribed = m_pCurrentItem->IsSubscribed();
	
	// Check if item is actually on disk using Steam API
	bool bOnDisk = false;
	ISteamUGC* pUGC = steamapicontext ? steamapicontext->SteamUGC() : NULL;
	if (pUGC)
	{
		uint32 itemState = pUGC->GetItemState(m_pCurrentItem->GetFileID());
		bOnDisk = (itemState & k_EItemStateInstalled) != 0;
	}
	
	if (m_pSubscribeButton)
	{
		m_pSubscribeButton->SetText("Subscribe");
		m_pSubscribeButton->SetVisible(!bSubscribed);
	}
	if (m_pUnsubscribeButton)
	{
		m_pUnsubscribeButton->SetText("Unsubscribe");
		m_pUnsubscribeButton->SetVisible(bSubscribed);
	}
	if (m_pOpenWorkshopButton)
		m_pOpenWorkshopButton->SetVisible(true);
	
	// Enable checkbox - only visible for subscribed items that are on disk
	if (m_pEnableCheckbox)
	{
		bool bEnabled = CFWorkshop()->IsItemEnabled(m_pCurrentItem->GetFileID());
		m_pEnableCheckbox->SetSelected(bEnabled);
		m_pEnableCheckbox->SetVisible(bSubscribed && bOnDisk);
		m_pEnableCheckbox->SetEnabled(bSubscribed && bOnDisk);
	}
}

void CCFWorkshopItemDetails::RequestPreviewImage(const char* pszURL)
{
	ISteamHTTP* pHTTP = SteamHTTP();
	if (!pHTTP || !pszURL || !pszURL[0])
	{
		CFWorkshopWarning("RequestPreviewImage: Invalid HTTP or URL\n");
		return;
	}
	
	CFWorkshopMsg("Requesting preview image: %s\n", pszURL);
	
	// Cancel any pending request
	if (m_hPendingPreviewRequest != INVALID_HTTPREQUEST_HANDLE)
	{
		pHTTP->ReleaseHTTPRequest(m_hPendingPreviewRequest);
		m_hPendingPreviewRequest = INVALID_HTTPREQUEST_HANDLE;
	}
	
	// Create the HTTP request
	m_hPendingPreviewRequest = pHTTP->CreateHTTPRequest(k_EHTTPMethodGET, pszURL);
	if (m_hPendingPreviewRequest == INVALID_HTTPREQUEST_HANDLE)
	{
		CFWorkshopWarning("Failed to create HTTP request for preview image\n");
		return;
	}
	
	// Set a reasonable timeout
	pHTTP->SetHTTPRequestNetworkActivityTimeout(m_hPendingPreviewRequest, 30);
	
	// Send the request
	SteamAPICall_t hCall;
	if (pHTTP->SendHTTPRequest(m_hPendingPreviewRequest, &hCall))
	{
		m_callbackHTTPPreview.Set(hCall, this, &CCFWorkshopItemDetails::Steam_OnPreviewImageReceived);
	}
	else
	{
		pHTTP->ReleaseHTTPRequest(m_hPendingPreviewRequest);
		m_hPendingPreviewRequest = INVALID_HTTPREQUEST_HANDLE;
		CFWorkshopWarning("Failed to send HTTP request for preview image\n");
	}
}

void CCFWorkshopItemDetails::Steam_OnPreviewImageReceived(HTTPRequestCompleted_t* pResult, bool bError)
{
	ISteamHTTP* pHTTP = SteamHTTP();
	if (!pHTTP)
		return;
	
	CFWorkshopMsg("HTTP preview callback received (request: %u, error: %d, success: %d, status: %d)\n", 
		pResult->m_hRequest, bError ? 1 : 0, pResult->m_bRequestSuccessful ? 1 : 0, pResult->m_eStatusCode);
	
	// Check if this request is still valid
	if (pResult->m_hRequest != m_hPendingPreviewRequest)
	{
		CFWorkshopMsg("Request handle mismatch, ignoring\n");
		pHTTP->ReleaseHTTPRequest(pResult->m_hRequest);
		return;
	}
	
	m_hPendingPreviewRequest = INVALID_HTTPREQUEST_HANDLE;
	
	// Verify the current item still matches
	if (!m_pCurrentItem || m_pCurrentItem->GetFileID() != m_nPreviewFileID)
	{
		CFWorkshopMsg("Item changed, ignoring preview\n");
		pHTTP->ReleaseHTTPRequest(pResult->m_hRequest);
		return;
	}
	
	if (bError || !pResult->m_bRequestSuccessful || pResult->m_eStatusCode != k_EHTTPStatusCode200OK)
	{
		CFWorkshopWarning("HTTP preview image request failed: status %d\n", pResult->m_eStatusCode);
		pHTTP->ReleaseHTTPRequest(pResult->m_hRequest);
		return;
	}
	
	// Get the body size
	uint32 unBodySize = 0;
	if (!pHTTP->GetHTTPResponseBodySize(pResult->m_hRequest, &unBodySize) || unBodySize == 0)
	{
		CFWorkshopWarning("HTTP response has no body\n");
		pHTTP->ReleaseHTTPRequest(pResult->m_hRequest);
		return;
	}
	
	// Create a buffer for the image data
	CUtlBuffer imageBuffer(0, unBodySize, CUtlBuffer::READ_ONLY);
	imageBuffer.EnsureCapacity(unBodySize);
	
	if (!pHTTP->GetHTTPResponseBodyData(pResult->m_hRequest, (uint8*)imageBuffer.Base(), unBodySize))
	{
		CFWorkshopWarning("Failed to get HTTP response body data\n");
		pHTTP->ReleaseHTTPRequest(pResult->m_hRequest);
		return;
	}
	
	// Set the buffer size after we've written to it
	imageBuffer.SeekPut(CUtlBuffer::SEEK_HEAD, unBodySize);
	
	pHTTP->ReleaseHTTPRequest(pResult->m_hRequest);
	
	// Determine image format from header bytes
	byte* pData = (byte*)imageBuffer.Base();
	//ImageFileFormat eFormat = kImageFileFormat_JPG; // Default to JPEG
	
	CFWorkshopMsg("Image data: size=%u, first bytes: %02X %02X %02X %02X\n", 
		unBodySize, pData[0], pData[1], pData[2], pData[3]);
	
	// Determine file extension based on format
	const char* pszExt = ".jpg";
	
	// Look for PNG signature (89 50 4E 47)
	if (unBodySize >= 8 && pData[0] == 0x89 && pData[1] == 'P' && pData[2] == 'N' && pData[3] == 'G')
	{
		pszExt = ".png";
		CFWorkshopMsg("Detected PNG format\n");
	}
	// JPEG starts with FF D8
	else if (unBodySize >= 2 && pData[0] == 0xFF && pData[1] == 0xD8)
	{
		pszExt = ".jpg";
		CFWorkshopMsg("Detected JPEG format\n");
	}
	else
	{
		CFWorkshopWarning("Unknown image format: %02X %02X %02X %02X\n", pData[0], pData[1], pData[2], pData[3]);
		return;
	}
	
	// Save to a temp file because ImgUtl_LoadJPEGBitmapFromBuffer is not implemented
	char szTempPath[MAX_PATH];
	V_snprintf(szTempPath, sizeof(szTempPath), "%s/download/previews/workshop_preview_%llu%s", 
		engine->GetGameDirectory(), m_nPreviewFileID, pszExt);
	
	// Ensure the directory exists
	char szDirPath[MAX_PATH];
	V_snprintf(szDirPath, sizeof(szDirPath), "%s/download/previews", engine->GetGameDirectory());
	g_pFullFileSystem->CreateDirHierarchy(szDirPath, "GAME");
	
	// Write the image data to the temp file
	FileHandle_t hFile = g_pFullFileSystem->Open(szTempPath, "wb", "GAME");
	if (hFile)
	{
		g_pFullFileSystem->Write(pData, unBodySize, hFile);
		g_pFullFileSystem->Close(hFile);
		
		// Now load the image using the file-based loader
		int nWidth = 0, nHeight = 0;
		ConversionErrorType result;
		unsigned char* pRGBA = ImgUtl_ReadImageAsRGBA(szTempPath, nWidth, nHeight, result);
		
		if (result == CE_SUCCESS && pRGBA && nWidth > 0 && nHeight > 0)
		{
			CFWorkshopMsg("Loaded preview image: %dx%d\n", nWidth, nHeight);
			
			// Set the texture on our preview image wrapper
			if (m_pPreviewImageData)
			{
				m_pPreviewImageData->SetTextureRGBA(pRGBA, nWidth, nHeight);
				
				// Set our custom image on the ImagePanel
				if (m_pPreviewImage && m_pPreviewImageData->IsValid())
				{
					// Get the ImagePanel size and set our image to match
					int wide, tall;
					m_pPreviewImage->GetSize(wide, tall);
					m_pPreviewImageData->SetSize(wide, tall);
					
					// Set our IImage on the panel
					m_pPreviewImage->SetImage(m_pPreviewImageData);
					m_pPreviewImage->SetVisible(true);
				}
			}
			
			// Free the RGBA data
			free(pRGBA);
		}
		else
		{
			CFWorkshopWarning("Failed to decode preview image (error %d)\n", result);
		}
		
		// Optionally delete the temp file
		// g_pFullFileSystem->RemoveFile(szTempPath, "GAME");
	}
	else
	{
		CFWorkshopWarning("Failed to create temp file for preview image\n");
	}
}

//-----------------------------------------------------------------------------
// Workshop Browser Panel
//-----------------------------------------------------------------------------
CCFWorkshopBrowserPanel::CCFWorkshopBrowserPanel(Panel* parent, const char* panelName)
	: BaseClass(parent, panelName)
	, m_pItemList(NULL)
	, m_pSearchBox(NULL)
	, m_pFilterCombo(NULL)
	, m_pSearchButton(NULL)
	, m_pRefreshButton(NULL)
	, m_pMyItemsButton(NULL)
	, m_pUploadButton(NULL)
	, m_pCosmeticToolButton(NULL)
	, m_pWeaponToolButton(NULL)
	, m_pCloseButton(NULL)
	, m_pDetailsPanel(NULL)
	, m_nContextMenuFileID(0)
	, m_eCurrentFilter(CF_WORKSHOP_TYPE_OTHER)
	, m_bShowSubscribedOnly(true)
	, m_flNextUpdateTime(0.0f)
	, m_bFilterTagsDirty(true)
	, m_nLastItemCount(0)
{
	m_szCurrentTagFilter[0] = '\0';
	// Load TF2 client scheme for proper styling
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx(
		enginevgui->GetPanel(PANEL_CLIENTDLL), 
		"resource/ClientScheme.res", 
		"ClientScheme");
	SetScheme(scheme);
	SetProportional(false);
	
	SetDeleteSelfOnClose(true);
	SetSizeable(false);
	SetMoveable(true);
	SetTitleBarVisible(false);
	SetCloseButtonVisible(false);
	
	SetSize(950, 600);
	MoveToCenterOfScreen();
}

CCFWorkshopBrowserPanel::~CCFWorkshopBrowserPanel()
{
	if (m_hContextMenu.Get())
	{
		delete m_hContextMenu.Get();
		m_hContextMenu = NULL;
	}
}

void CCFWorkshopBrowserPanel::ApplySchemeSettings(IScheme* pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	
	// Load the TF2-style resource file - this creates controls
	LoadControlSettings( RES_BROWSER );
	
	// Find controls created by .res file
	m_pItemList = dynamic_cast<ListPanel*>(FindChildByName("ItemList"));
	m_pSearchBox = dynamic_cast<TextEntry*>(FindChildByName("SearchBox"));
	m_pFilterCombo = dynamic_cast<ComboBox*>(FindChildByName("FilterCombo"));
	m_pSearchButton = dynamic_cast<Button*>(FindChildByName("SearchButton"));
	m_pRefreshButton = dynamic_cast<Button*>(FindChildByName("RefreshButton"));
	m_pMyItemsButton = dynamic_cast<Button*>(FindChildByName("MyItemsButton"));
	m_pUploadButton = dynamic_cast<Button*>(FindChildByName("UploadButton"));
	m_pCosmeticToolButton = dynamic_cast<Button*>(FindChildByName("CosmeticToolButton"));
	m_pWeaponToolButton = dynamic_cast<Button*>(FindChildByName("WeaponToolButton"));
	m_pCloseButton = dynamic_cast<Button*>(FindChildByName("CloseButton"));
	
	// Find or create the details panel
	// The .res creates a DetailsPanel container with buttons inside it
	// We pass that container to our CCFWorkshopItemDetails which will find the controls
	EditablePanel* pDetailsContainer = dynamic_cast<EditablePanel*>(FindChildByName("DetailsPanel"));
	if (pDetailsContainer && !m_pDetailsPanel)
	{
		// Create our custom details panel - it will find controls from its parent (the container)
		// Make it NOT cover the container so buttons remain clickable
		m_pDetailsPanel = new CCFWorkshopItemDetails(this, "ItemDetails");
		m_pDetailsPanel->SetVisible(false); // We don't need it visible, we just use it to handle logic
		m_pDetailsPanel->SetParentContainer(pDetailsContainer);
	}
	
	// Setup filter combo - will be populated dynamically when items are loaded
	if (m_pFilterCombo)
	{
		m_pFilterCombo->RemoveAll();
		m_pFilterCombo->AddItem("All Items", NULL);
		m_pFilterCombo->ActivateItemByRow(0);
		// Add action signal target so we get notified of selection changes
		m_pFilterCombo->AddActionSignalTarget(this);
	}
	
	// Setup list columns if found
	if (m_pItemList)
	{
		m_pItemList->RemoveAll();
		m_pItemList->AddColumnHeader(0, "name", "Name", 250);
		m_pItemList->AddColumnHeader(1, "tags", "Tags", 150);
		m_pItemList->AddColumnHeader(2, "status", "Status", 100);
		m_pItemList->AddColumnHeader(3, "size", "Size", 100);
		m_pItemList->SetAllowUserModificationOfColumns(true);
		
		// Make the list send selection notifications to us
		m_pItemList->AddActionSignalTarget(this);
	}
}

void CCFWorkshopBrowserPanel::PerformLayout()
{
	BaseClass::PerformLayout();
	// Let .res file handle all positioning
}

void CCFWorkshopBrowserPanel::OnCommand(const char* command)
{
	if (Q_stricmp(command, "Close") == 0)
	{
		Close();
	}
	else if (Q_stricmp(command, "search") == 0)
	{
		if (m_pSearchBox && CFWorkshop())
		{
			char szSearch[256];
			m_pSearchBox->GetText(szSearch, sizeof(szSearch));
			CFWorkshop()->SearchWorkshop(szSearch, m_eCurrentFilter);
		}
	}
	else if (Q_stricmp(command, "refresh") == 0)
	{
		if (CFWorkshop() && !CFWorkshop()->IsQueryInProgress())
		{
			CFWorkshop()->RefreshSubscriptions();
			CFWorkshop()->QueryRecentItems(35);
			m_bFilterTagsDirty = true; // New query means tags may change
		}
		RefreshList();
	}
	else if (Q_stricmp(command, "myitems") == 0)
	{
		if (CFWorkshop())
		{
			CFWorkshop()->QueryUserItems(1);
		}
	}
	else if (Q_stricmp(command, "upload") == 0)
	{
		// Open the upload dialog
		CCFWorkshopUploadDialog* pDialog = new CCFWorkshopUploadDialog(this, "WorkshopUploadDialog");
		pDialog->SetupForNewItem(CF_WORKSHOP_TYPE_MAP);
		pDialog->DoModal();
	}
	else if (Q_stricmp(command, "cosmetic_tool") == 0)
	{
		// Open the cosmetic tool
		engine->ClientCmd_Unrestricted("cf_workshop_cosmetic_tool");
	}
	else if (Q_stricmp(command, "weapon_tool") == 0)
	{
		// Open the weapon tool
		engine->ClientCmd_Unrestricted("cf_workshop_weapon_tool");
	}
	else if (Q_stricmp(command, "ctx_update") == 0)
	{
		// Open the update metadata dialog
		if (m_nContextMenuFileID != 0)
		{
			CCFWorkshopUpdateDialog* pDialog = new CCFWorkshopUpdateDialog(this, "WorkshopUpdateDialog");
			pDialog->SetupForItem(m_nContextMenuFileID);
			pDialog->DoModal();
		}
	}
	else if (Q_stricmp(command, "ctx_delete") == 0)
	{
		// Open the delete confirmation dialog
		if (m_nContextMenuFileID != 0)
		{
			CCFWorkshopItem* pItem = CFWorkshop() ? CFWorkshop()->GetItem(m_nContextMenuFileID) : NULL;
			CCFWorkshopDeleteConfirmDialog* pDialog = new CCFWorkshopDeleteConfirmDialog(this, "WorkshopDeleteDialog");
			pDialog->SetupForItem(m_nContextMenuFileID, pItem ? pItem->GetTitle() : "Unknown Item");
			pDialog->DoModal();
		}
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}

void CCFWorkshopBrowserPanel::OnThink()
{
	BaseClass::OnThink();
	
	// Periodic update
	if (gpGlobals->curtime > m_flNextUpdateTime)
	{
		RefreshList();
		m_flNextUpdateTime = gpGlobals->curtime + 2.0f; // Update every 2 seconds
	}
}

void CCFWorkshopBrowserPanel::ShowPanel(bool bShow)
{
	if (bShow)
	{
		Activate();
		// Start a query for recent workshop items
		if (CFWorkshop() && !CFWorkshop()->IsQueryInProgress())
		{
			CFWorkshop()->QueryRecentItems(35);
			m_bFilterTagsDirty = true; // New query means tags may change
		}
		RefreshList();
	}
	else
	{
		SetVisible(false);
	}
}

void CCFWorkshopBrowserPanel::PopulateFilterTags()
{
	if (!CFWorkshop() || !m_pFilterCombo)
		return;
	
	// Collect all unique tags from browseable items
	CUtlVector<CUtlString> uniqueTags;
	uint32 numItems = CFWorkshop()->GetBrowseableItemCount();
	
	for (uint32 i = 0; i < numItems; i++)
	{
		CCFWorkshopItem* pItem = CFWorkshop()->GetBrowseableItem(i);
		if (!pItem)
			continue;
		
		const char* pszTags = pItem->GetTags();
		if (!pszTags || !pszTags[0])
			continue;
		
		// Split tags by comma and add unique ones
		char szTagsCopy[256];
		V_strncpy(szTagsCopy, pszTags, sizeof(szTagsCopy));
		
		char* token = strtok(szTagsCopy, ",");
		while (token != NULL)
		{
			// Trim whitespace
			while (*token == ' ') token++;
			char* end = token + strlen(token) - 1;
			while (end > token && *end == ' ') *end-- = '\0';
			
			// Check if we already have this tag
			bool bFound = false;
			for (int j = 0; j < uniqueTags.Count(); j++)
			{
				if (V_stricmp(uniqueTags[j].Get(), token) == 0)
				{
					bFound = true;
					break;
				}
			}
			
			if (!bFound && token[0] != '\0')
			{
				uniqueTags.AddToTail(token);
			}
			
			token = strtok(NULL, ",");
		}
	}
	
	// Sort tags alphabetically
	uniqueTags.Sort([](const CUtlString* a, const CUtlString* b) -> int {
		return V_stricmp(a->Get(), b->Get());
	});
	
	// Add sorted tags to combo box
	for (int i = 0; i < uniqueTags.Count(); i++)
	{
		m_pFilterCombo->AddItem(uniqueTags[i].Get(), NULL);
	}
}

void CCFWorkshopBrowserPanel::RefreshList()
{
	if (!CFWorkshop() || !m_pItemList)
		return;
	
	// Check if item count has changed - if so, we need to rebuild filter tags
	uint32 numItems = CFWorkshop()->GetBrowseableItemCount();
	if (numItems != m_nLastItemCount)
	{
		m_bFilterTagsDirty = true;
		m_nLastItemCount = numItems;
	}
	
	// Only update filter tags when they're actually dirty (new items loaded, etc.)
	// Don't rebuild the combo on every periodic refresh - this closes the dropdown
	if (m_pFilterCombo && m_bFilterTagsDirty)
	{
		int currentSelection = m_pFilterCombo->GetActiveItem();
		char szCurrentText[256] = {0};
		if (currentSelection >= 0)
		{
			m_pFilterCombo->GetItemText(currentSelection, szCurrentText, sizeof(szCurrentText));
		}
		
		// Rebuild the filter list
		m_pFilterCombo->RemoveAll();
		m_pFilterCombo->AddItem("All Items", NULL);
		PopulateFilterTags();
		
		// Try to restore previous selection
		if (szCurrentText[0] != '\0')
		{
			for (int i = 0; i < m_pFilterCombo->GetItemCount(); i++)
			{
				char szItemText[256];
				m_pFilterCombo->GetItemText(i, szItemText, sizeof(szItemText));
				if (V_stricmp(szItemText, szCurrentText) == 0)
				{
					m_pFilterCombo->ActivateItemByRow(i);
					break;
				}
			}
		}
		
		if (m_pFilterCombo->GetActiveItem() < 0)
		{
			m_pFilterCombo->ActivateItemByRow(0);
		}
		
		m_bFilterTagsDirty = false; // Mark as clean after rebuilding
	}
		
	m_pItemList->RemoveAll();
	
	// Populate item list (numItems already retrieved above)
	for (uint32 i = 0; i < numItems; i++)
	{
		CCFWorkshopItem* pItem = CFWorkshop()->GetBrowseableItem(i);
		
		// Verify item still exists and is valid
		if (!pItem || !pItem->GetTitle() || !pItem->GetTitle()[0])
			continue;
		
		// Apply tag filter if one is set
		if (m_szCurrentTagFilter[0] != '\0' && V_stricmp(m_szCurrentTagFilter, "All Items") != 0)
		{
			// Check if this item has the selected tag
			const char* pszItemTags = pItem->GetTags();
			if (!pszItemTags || !V_stristr(pszItemTags, m_szCurrentTagFilter))
			{
				// Item doesn't match filter, skip it
				continue;
			}
		}
		
		KeyValues* kv = new KeyValues("item");
		kv->SetString("name", pItem->GetTitle());
		
		// Show tags instead of type
		const char* pszTags = pItem->GetTags();
		if (!pszTags || !pszTags[0])
			pszTags = "-";
		kv->SetString("tags", pszTags);
		
		// Show subscription status instead of download state
		const char* pszState = pItem->IsSubscribed() ? "Subscribed" : "Not Subscribed";
		if (pItem->GetState() == CF_WORKSHOP_STATE_DOWNLOADING)
			pszState = "Downloading";
		kv->SetString("status", pszState);
		
		float sizeMB = pItem->GetFileSize() / (1024.0f * 1024.0f);
		char szSize[32];
		V_snprintf(szSize, sizeof(szSize), "%.1f MB", sizeMB);
		kv->SetString("size", szSize);
		
		/*int itemID = */m_pItemList->AddItem(kv, (unsigned int)pItem->GetFileID(), false, false);
		kv->deleteThis();
	}
	
	// If no items yet (query in progress), show a message
	if (numItems == 0 && CFWorkshop()->IsQueryInProgress())
	{
		KeyValues* kv = new KeyValues("item");
		kv->SetString("name", "Loading workshop items...");
		kv->SetString("type", "");
		kv->SetString("status", "");
		kv->SetString("size", "");
		m_pItemList->AddItem(kv, 0, false, false);
		kv->deleteThis();
	}
}

void CCFWorkshopBrowserPanel::OnItemSelected()
{
	if (!m_pItemList || !CFWorkshop())
		return;
	
	// Get the currently selected item from the list
	int selectedItem = m_pItemList->GetSelectedItem(0);
	if (selectedItem < 0)
		return;
		
	unsigned int fileID = m_pItemList->GetItemUserData(selectedItem);
	CCFWorkshopItem* pItem = CFWorkshop()->GetItem(fileID);
	
	if (m_pDetailsPanel && pItem)
	{
		m_pDetailsPanel->SetItem(pItem);
	}
}

void CCFWorkshopBrowserPanel::PopulateItemList()
{
	RefreshList();
}

void CCFWorkshopBrowserPanel::ApplyFilters()
{
	// Filtering is now handled directly in RefreshList()
	RefreshList();
}

void CCFWorkshopBrowserPanel::UpdateItemRow(int itemID, CCFWorkshopItem* pItem)
{
	// TODO: Update specific row
}

void CCFWorkshopBrowserPanel::OnOpenContextMenu(int itemID)
{
	if (!CFWorkshop() || !m_pItemList)
		return;
	
	// Get the file ID from the item
	unsigned int fileID = m_pItemList->GetItemUserData(itemID);
	if (fileID == 0)
		return;
	
	CCFWorkshopItem* pItem = CFWorkshop()->GetItem(fileID);
	if (!pItem)
		return;
	
	// Only show context menu for items owned by the current user
	if (!pItem->IsOwnedByCurrentUser())
		return;
	
	ShowContextMenu(itemID);
}

void CCFWorkshopBrowserPanel::ShowContextMenu(int itemID)
{
	if (!m_pItemList)
		return;
	
	// Get the file ID from the item
	unsigned int fileID = m_pItemList->GetItemUserData(itemID);
	m_nContextMenuFileID = fileID;
	
	// Clean up old menu
	if (m_hContextMenu.Get())
	{
		delete m_hContextMenu.Get();
	}
	
	// Create context menu
	m_hContextMenu = new Menu(this, "WorkshopContextMenu");
	m_hContextMenu->AddMenuItem("Update Item...", "ctx_update", this);
	m_hContextMenu->AddMenuItem("Delete Item...", "ctx_delete", this);
	
	// Position at cursor
	int x, y;
	input()->GetCursorPos(x, y);
	m_hContextMenu->SetPos(x, y);
	m_hContextMenu->SetVisible(true);
}

void CCFWorkshopBrowserPanel::OnTextChanged(KeyValues* data)
{
	// Handle ComboBox selection changes
	Panel* pPanel = reinterpret_cast<Panel*>(data->GetPtr("panel"));
	if (!pPanel || pPanel != m_pFilterCombo)
		return;
	
	if (!m_pFilterCombo)
		return;
	
	// Get the newly selected filter text
	int activeItem = m_pFilterCombo->GetActiveItem();
	if (activeItem >= 0)
	{
		m_pFilterCombo->GetItemText(activeItem, m_szCurrentTagFilter, sizeof(m_szCurrentTagFilter));
		
		// Apply the new filter
		ApplyFilters();
	}
}

//-----------------------------------------------------------------------------
// Workshop Upload Dialog
//-----------------------------------------------------------------------------
CCFWorkshopUploadDialog::CCFWorkshopUploadDialog(Panel* parent, const char* panelName)
	: BaseClass(parent, panelName)
	, m_nFileID(0)
	, m_eItemType(CF_WORKSHOP_TYPE_MAP)
	, m_bIsUpdate(false)
	, m_eBrowsingType(BROWSE_CONTENT)
	, m_pTitleEntry(NULL)
	, m_pDescriptionEntry(NULL)
	, m_pTagsEntry(NULL)
	, m_pContentPathEntry(NULL)
	, m_pPreviewImageEntry(NULL)
	, m_pScreenshotsFolderEntry(NULL)
	, m_pBrowseContentButton(NULL)
	, m_pBrowsePreviewButton(NULL)
	, m_pBrowseScreenshotsButton(NULL)
	, m_pSubmitButton(NULL)
	, m_pCancelButton(NULL)
	, m_pTypeCombo(NULL)
	, m_pVisibilityCombo(NULL)
	, m_pStatusLabel(NULL)
	, m_pProgressBar(NULL)
	, m_pPreviewImagePanel(NULL)
	, m_pPreviewImage(NULL)
{
	// Create preview image object
	m_pPreviewImage = new CWorkshopPreviewImage();
	
	// Initialize tag checkboxes array
	for (int i = 0; i < CF_WORKSHOP_TAG_COUNT; i++)
	{
		m_pTagChecks[i] = NULL;
	}
	
	// Load TF2 client scheme for proper styling
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx(
		enginevgui->GetPanel(PANEL_CLIENTDLL), 
		"resource/ClientScheme.res", 
		"ClientScheme");
	SetScheme(scheme);
	SetProportional(false);
	
	SetDeleteSelfOnClose(true);
	SetSizeable(false);
	SetMoveable(true);
	SetTitleBarVisible(false);
	SetCloseButtonVisible(false);

	MoveToCenterOfScreen();
}

CCFWorkshopUploadDialog::~CCFWorkshopUploadDialog()
{
	if (m_pPreviewImage)
	{
		delete m_pPreviewImage;
		m_pPreviewImage = NULL;
	}
}

void CCFWorkshopUploadDialog::ApplySchemeSettings(IScheme* pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	
	// Load the TF2-style resource file - creates all controls
	LoadControlSettings( RES_UPLOAD );
	
	// Force size since .res might not be applying correctly
	SetSize(900, 760);
	
	// Find controls created by .res file
	m_pTitleEntry = dynamic_cast<TextEntry*>(FindChildByName("TitleEntry"));
	m_pDescriptionEntry = dynamic_cast<TextEntry*>(FindChildByName("DescriptionEntry"));
	m_pContentPathEntry = dynamic_cast<TextEntry*>(FindChildByName("ContentPathEntry"));
	m_pPreviewImageEntry = dynamic_cast<TextEntry*>(FindChildByName("PreviewImageEntry"));
	m_pScreenshotsFolderEntry = dynamic_cast<TextEntry*>(FindChildByName("ScreenshotsFolderEntry"));
	m_pBrowseContentButton = dynamic_cast<Button*>(FindChildByName("BrowseContentButton"));
	m_pBrowsePreviewButton = dynamic_cast<Button*>(FindChildByName("BrowsePreviewButton"));
	m_pBrowseScreenshotsButton = dynamic_cast<Button*>(FindChildByName("BrowseScreenshotsButton"));
	m_pSubmitButton = dynamic_cast<Button*>(FindChildByName("UploadButton"));
	m_pCancelButton = dynamic_cast<Button*>(FindChildByName("CancelButton"));
	m_pTypeCombo = dynamic_cast<ComboBox*>(FindChildByName("TypeCombo"));
	m_pVisibilityCombo = dynamic_cast<ComboBox*>(FindChildByName("VisibilityCombo"));
	m_pStatusLabel = dynamic_cast<Label*>(FindChildByName("StatusLabel"));
	m_pProgressBar = dynamic_cast<ProgressBar*>(FindChildByName("ProgressBar"));
	m_pPreviewImagePanel = dynamic_cast<ImagePanel*>(FindChildByName("PreviewImagePanel"));
	
	// Initialize tag checkboxes array to NULL
	for (int i = 0; i < CF_WORKSHOP_TAG_COUNT; i++)
	{
		m_pTagChecks[i] = NULL;
	}
	
	// Find tag checkboxes by name - names must match the .res file (TagCheck_XXX format)
	// MOD TYPE category
	m_pTagChecks[CF_WORKSHOP_TAGIDX_MAP] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Map"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_SKIN] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Skin"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_WEAPON] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Weapon"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_GUIS] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_GUIs"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_PARTICLES] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Particles"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_SOUNDS] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Sounds"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_SPRAYS] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Sprays"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_MISC] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Misc"));
	
	// GAME MODE category
	m_pTagChecks[CF_WORKSHOP_TAGIDX_CTF] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_CTF"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_CP] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_CP"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_PAYLOAD] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Payload"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_PLR] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_PLR"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_ARENA] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Arena"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_KOTH] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_KOTH"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_AD] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_AD"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_SD] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_SD"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_RD] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_RD"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_MVM] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_MVM"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_MVM_VERSUS] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_MVMVersus"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_MANNPOWER] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Mannpower"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_MEDIEVAL] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Medieval"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_PASS] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_PASS"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_PD] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_PD"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_CUSTOM] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Custom"));
	
	// CLASS category
	m_pTagChecks[CF_WORKSHOP_TAGIDX_SCOUT] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Scout"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_SOLDIER] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Soldier"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_PYRO] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Pyro"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_DEMOMAN] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Demoman"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_HEAVY] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Heavy"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_ENGINEER] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Engineer"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_MEDIC] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Medic"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_SNIPER] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Sniper"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_SPY] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Spy"));
	
	// ITEM SLOT category
	m_pTagChecks[CF_WORKSHOP_TAGIDX_PRIMARY] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Primary"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_SECONDARY] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Secondary"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_MELEE] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Melee"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_PDA] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_PDA"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_COSMETIC] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Cosmetic"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_ACTION] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Action"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_TAUNT] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Taunt"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_SLOT_SKIN] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_SlotSkin"));
	
	// OTHER category
	m_pTagChecks[CF_WORKSHOP_TAGIDX_HALLOWEEN] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Halloween"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_CHRISTMAS] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Christmas"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_UNUSUAL] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Unusual"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_WARPAINT] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_WarPaint"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_KILLSTREAK] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Killstreak"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_COMMUNITY_FIX] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_CommunityFix"));
	
	// Setup description entry
	if (m_pDescriptionEntry)
	{
		m_pDescriptionEntry->SetMultiline(true);
		m_pDescriptionEntry->SetCatchEnterKey(true);
		m_pDescriptionEntry->SetVerticalScrollbar(true);
	}
	
	// Setup type combo
	if (m_pTypeCombo)
	{
		m_pTypeCombo->RemoveAll();
		m_pTypeCombo->AddItem("Map", NULL);
		m_pTypeCombo->AddItem("Weapon Skin", NULL);
		m_pTypeCombo->AddItem("Weapon Mod", NULL);
		m_pTypeCombo->AddItem("Character Skin", NULL);
		m_pTypeCombo->AddItem("Particle Effect", NULL);
		m_pTypeCombo->AddItem("Sound Mod", NULL);
		m_pTypeCombo->AddItem("HUD", NULL);
		m_pTypeCombo->AddItem("Other", NULL);
		m_pTypeCombo->ActivateItemByRow(0);
	}
	
	// Setup visibility combo
	if (m_pVisibilityCombo)
	{
		m_pVisibilityCombo->RemoveAll();
		m_pVisibilityCombo->AddItem("Public", NULL);
		m_pVisibilityCombo->AddItem("Friends Only", NULL);
		m_pVisibilityCombo->AddItem("Private", NULL);
		m_pVisibilityCombo->ActivateItemByRow(0);
	}
	
	// Center dialog after loading .res settings
	MoveToCenterOfScreen();
}

void CCFWorkshopUploadDialog::PerformLayout()
{
	BaseClass::PerformLayout();
	// Let .res file handle all positioning
}

void CCFWorkshopUploadDialog::OnThink()
{
	BaseClass::OnThink();
	
	// Update progress if uploading
	if (CFWorkshop() && CFWorkshop()->IsUploading())
	{
		if (m_pProgressBar)
		{
			m_pProgressBar->SetVisible(true);
			m_pProgressBar->SetProgress(CFWorkshop()->GetUploadProgress());
		}
		if (m_pStatusLabel)
			m_pStatusLabel->SetText(CFWorkshop()->GetUploadStatus());
		if (m_pSubmitButton)
			m_pSubmitButton->SetEnabled(false);
	}
	else
	{
		if (m_pSubmitButton)
			m_pSubmitButton->SetEnabled(true);
		
		// Check if upload just completed
		const char* status = CFWorkshop() ? CFWorkshop()->GetUploadStatus() : "";
		if (status[0] != '\0')
		{
			if (m_pStatusLabel)
				m_pStatusLabel->SetText(status);
			
			// If successful, show the progress bar at 100%
			if (V_strstr(status, "complete") != NULL && m_pProgressBar)
			{
				m_pProgressBar->SetVisible(true);
				m_pProgressBar->SetProgress(1.0f);
			}
		}
	}
}

void CCFWorkshopUploadDialog::OnCommand(const char* command)
{
	if (Q_stricmp(command, "submit") == 0 || Q_stricmp(command, "upload") == 0)
	{
		DoUpload();
	}
	else if (Q_stricmp(command, "cancel") == 0)
	{
		Close();
	}
	else if (Q_stricmp(command, "browsecontent") == 0)
	{
		OpenContentFolderBrowser();
	}
	else if (Q_stricmp(command, "browsepreview") == 0)
	{
		OpenPreviewFileBrowser();
	}
	else if (Q_stricmp(command, "browsescreenshots") == 0)
	{
		OpenScreenshotsFolderBrowser();
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}

void CCFWorkshopUploadDialog::DoUpload()
{
	if (!CFWorkshop())
	{
		if (m_pStatusLabel)
			m_pStatusLabel->SetText("Error: Workshop system not initialized");
		return;
	}
	
	if (CFWorkshop()->IsUploading())
	{
		if (m_pStatusLabel)
			m_pStatusLabel->SetText("Upload already in progress...");
		return;
	}
	
	// Get values from UI
	char szTitle[256] = {0};
	char szDescription[2048] = {0};
	char szContentPath[MAX_PATH] = {0};
	char szPreviewPath[MAX_PATH] = {0};
	char szScreenshotsPath[MAX_PATH] = {0};
	
	if (m_pTitleEntry) m_pTitleEntry->GetText(szTitle, sizeof(szTitle));
	if (m_pDescriptionEntry) m_pDescriptionEntry->GetText(szDescription, sizeof(szDescription));
	if (m_pContentPathEntry) m_pContentPathEntry->GetText(szContentPath, sizeof(szContentPath));
	if (m_pPreviewImageEntry) m_pPreviewImageEntry->GetText(szPreviewPath, sizeof(szPreviewPath));
	if (m_pScreenshotsFolderEntry) m_pScreenshotsFolderEntry->GetText(szScreenshotsPath, sizeof(szScreenshotsPath));
	
	// Validate title
	if (!szTitle[0])
	{
		if (m_pStatusLabel)
			m_pStatusLabel->SetText("Error: Title is required");
		return;
	}
	
	// Validate content path
	if (!szContentPath[0])
	{
		if (m_pStatusLabel)
			m_pStatusLabel->SetText("Error: Content folder path is required");
		return;
	}
	
	// Get type from combo
	CFWorkshopItemType_t type = CF_WORKSHOP_TYPE_MAP;
	if (m_pTypeCombo)
	{
		int typeRow = m_pTypeCombo->GetActiveItem();
		switch (typeRow)
		{
			case 0: type = CF_WORKSHOP_TYPE_MAP; break;
			case 1: type = CF_WORKSHOP_TYPE_WEAPON_SKIN; break;
			case 2: type = CF_WORKSHOP_TYPE_WEAPON_MOD; break;
			case 3: type = CF_WORKSHOP_TYPE_CHARACTER_SKIN; break;
			case 4: type = CF_WORKSHOP_TYPE_PARTICLE_EFFECT; break;
			case 5: type = CF_WORKSHOP_TYPE_SOUND_MOD; break;
			case 6: type = CF_WORKSHOP_TYPE_HUD; break;
			default: type = CF_WORKSHOP_TYPE_OTHER; break;
		}
	}
	
	// Get visibility from combo
	ERemoteStoragePublishedFileVisibility visibility = k_ERemoteStoragePublishedFileVisibilityPublic;
	if (m_pVisibilityCombo)
	{
		int visRow = m_pVisibilityCombo->GetActiveItem();
		switch (visRow)
		{
			case 0: visibility = k_ERemoteStoragePublishedFileVisibilityPublic; break;
			case 1: visibility = k_ERemoteStoragePublishedFileVisibilityFriendsOnly; break;
			case 2: visibility = k_ERemoteStoragePublishedFileVisibilityPrivate; break;
		}
	}
	
	// Build tag string from selected checkboxes
	char szTags[1024] = {0};
	BuildTagString(szTags, sizeof(szTags));
	
	// Start upload
	if (m_pStatusLabel)
		m_pStatusLabel->SetText("Starting upload...");
	if (m_pProgressBar)
	{
		m_pProgressBar->SetVisible(true);
		m_pProgressBar->SetProgress(0.0f);
	}
	
	bool bSuccess = CFWorkshop()->StartUpload(
		szTitle,
		szDescription,
		szContentPath,
		szPreviewPath[0] ? szPreviewPath : NULL,
		type,
		visibility,
		m_bIsUpdate ? m_nFileID : 0,
		szTags[0] ? szTags : NULL,
		szScreenshotsPath[0] ? szScreenshotsPath : NULL
	);
	
	if (!bSuccess && m_pStatusLabel)
	{
		m_pStatusLabel->SetText(CFWorkshop()->GetUploadStatus());
	}
}

void CCFWorkshopUploadDialog::BuildTagString(char* pszOut, int maxLen)
{
	pszOut[0] = '\0';
	
	bool bFirst = true;
	for (int i = 0; i < CF_WORKSHOP_TAG_COUNT; i++)
	{
		if (m_pTagChecks[i] && m_pTagChecks[i]->IsSelected())
		{
			// Get the tag name from the global tag info array
			const char* pszTagName = g_CFWorkshopTags[i].pszTagName;
			
			if (bFirst)
			{
				V_strncpy(pszOut, pszTagName, maxLen);
				bFirst = false;
			}
			else
			{
				V_strncat(pszOut, ",", maxLen);
				V_strncat(pszOut, pszTagName, maxLen);
			}
		}
	}
}

void CCFWorkshopUploadDialog::SetupForNewItem(CFWorkshopItemType_t type)
{
	m_eItemType = type;
	m_bIsUpdate = false;
	m_nFileID = 0;
	
	SetTitle("Upload New Item to Workshop", true);
	
	if (m_pTitleEntry)
		m_pTitleEntry->SetText("");
	if (m_pDescriptionEntry)
		m_pDescriptionEntry->SetText("");
	if (m_pContentPathEntry)
		m_pContentPathEntry->SetText("");
	if (m_pPreviewImageEntry)
		m_pPreviewImageEntry->SetText("");
	if (m_pStatusLabel)
		m_pStatusLabel->SetText("");
	if (m_pProgressBar)
	{
		m_pProgressBar->SetVisible(false);
		m_pProgressBar->SetProgress(0.0f);
	}
	
	// Clear all tag checkboxes
	for (int i = 0; i < CF_WORKSHOP_TAG_COUNT; i++)
	{
		if (m_pTagChecks[i])
			m_pTagChecks[i]->SetSelected(false);
	}
	
	// Set type combo to match
	if (m_pTypeCombo)
	{
		int row = 0;
		switch (type)
		{
			case CF_WORKSHOP_TYPE_MAP: row = 0; break;
			case CF_WORKSHOP_TYPE_WEAPON_SKIN: row = 1; break;
			case CF_WORKSHOP_TYPE_WEAPON_MOD: row = 2; break;
			case CF_WORKSHOP_TYPE_CHARACTER_SKIN: row = 3; break;
			case CF_WORKSHOP_TYPE_PARTICLE_EFFECT: row = 4; break;
			case CF_WORKSHOP_TYPE_SOUND_MOD: row = 5; break;
			case CF_WORKSHOP_TYPE_HUD: row = 6; break;
			default: row = 7; break;
		}
		m_pTypeCombo->ActivateItemByRow(row);
	}
}

void CCFWorkshopUploadDialog::SetupForUpdate(PublishedFileId_t fileID)
{
	m_nFileID = fileID;
	m_bIsUpdate = true;
	
	char szTitle[128];
	V_snprintf(szTitle, sizeof(szTitle), "Update Workshop Item %llu", fileID);
	SetTitle(szTitle, true);
	
	if (m_pStatusLabel)
		m_pStatusLabel->SetText("");
	if (m_pProgressBar)
	{
		m_pProgressBar->SetVisible(false);
		m_pProgressBar->SetProgress(0.0f);
	}
	
	// If we have the item, fill in current values
	CCFWorkshopItem* pItem = CFWorkshop() ? CFWorkshop()->GetItem(fileID) : NULL;
	if (pItem)
	{
		if (m_pTitleEntry)
			m_pTitleEntry->SetText(pItem->GetTitle());
		if (m_pDescriptionEntry)
			m_pDescriptionEntry->SetText(pItem->GetDescription());
		m_eItemType = pItem->GetType();
	}
}

void CCFWorkshopUploadDialog::OpenContentFolderBrowser()
{
	m_eBrowsingType = BROWSE_CONTENT;
	
	FileOpenDialog* pDialog = new FileOpenDialog(this, "Select Content Folder", FOD_SELECT_DIRECTORY);
	// Set start directory to the game root (Custom Fortress directory)
	char szGameDir[MAX_PATH];
	V_snprintf(szGameDir, sizeof(szGameDir), "%s", engine->GetGameDirectory());
	pDialog->SetStartDirectory(szGameDir);
	pDialog->AddActionSignalTarget(this);
	
	// Make background solid
	pDialog->SetPaintBackgroundEnabled(true);
	pDialog->SetPaintBackgroundType(0); // Square background
	pDialog->SetBgColor(Color(46, 43, 42, 255));
	
	pDialog->DoModal();
}

void CCFWorkshopUploadDialog::OpenPreviewFileBrowser()
{
	m_eBrowsingType = BROWSE_PREVIEW;
	
	FileOpenDialog* pDialog = new FileOpenDialog(this, "Select Preview Image", FOD_OPEN);
	// Set start directory to the game root (Custom Fortress directory)
	char szGameDir[MAX_PATH];
	V_snprintf(szGameDir, sizeof(szGameDir), "%s", engine->GetGameDirectory());
	pDialog->SetStartDirectory(szGameDir);
	pDialog->AddFilter("*.jpg;*.jpeg;*.png", "Image Files (*.jpg, *.png)", true);
	pDialog->AddFilter("*.*", "All Files (*.*)", false);
	pDialog->AddActionSignalTarget(this);
	
	// Make background solid
	pDialog->SetPaintBackgroundEnabled(true);
	pDialog->SetPaintBackgroundType(0); // Square background
	pDialog->SetBgColor(Color(46, 43, 42, 255));
	
	pDialog->DoModal();
}

void CCFWorkshopUploadDialog::OpenScreenshotsFolderBrowser()
{
	m_eBrowsingType = BROWSE_SCREENSHOTS;
	
	FileOpenDialog* pDialog = new FileOpenDialog(this, "Select Screenshots Folder", FOD_SELECT_DIRECTORY);
	// Set start directory to the game root (Custom Fortress directory)
	char szGameDir[MAX_PATH];
	V_snprintf(szGameDir, sizeof(szGameDir), "%s", engine->GetGameDirectory());
	pDialog->SetStartDirectory(szGameDir);
	pDialog->AddActionSignalTarget(this);
	
	// Make background solid
	pDialog->SetPaintBackgroundEnabled(true);
	pDialog->SetPaintBackgroundType(0); // Square background
	pDialog->SetBgColor(Color(46, 43, 42, 255));
	
	pDialog->DoModal();
}

void CCFWorkshopUploadDialog::OnFileSelected(KeyValues* params)
{
	const char* pszFullPath = params->GetString("fullpath", "");
	
	if (pszFullPath[0])
	{
		switch (m_eBrowsingType)
		{
			case BROWSE_CONTENT:
				// Content folder selected
				if (m_pContentPathEntry)
					m_pContentPathEntry->SetText(pszFullPath);
				if (m_pStatusLabel)
					m_pStatusLabel->SetText("Content folder selected");
				break;
				
			case BROWSE_PREVIEW:
				// Preview image selected
				if (m_pPreviewImageEntry)
					m_pPreviewImageEntry->SetText(pszFullPath);
				if (m_pStatusLabel)
					m_pStatusLabel->SetText("Preview image selected");
				
				// Load and display the preview image
				if (m_pPreviewImagePanel && m_pPreviewImage)
				{
					if (m_pPreviewImage->SetTextureFromFile(pszFullPath))
					{
						// Size the image to fit the panel
						int panelWide, panelTall;
						m_pPreviewImagePanel->GetSize(panelWide, panelTall);
						m_pPreviewImage->SetSize(panelWide, panelTall);
						m_pPreviewImagePanel->SetImage(m_pPreviewImage);
					}
					else
					{
						if (m_pStatusLabel)
							m_pStatusLabel->SetText("Failed to load preview image");
					}
				}
				break;
				
			case BROWSE_SCREENSHOTS:
				// Screenshots folder selected
				if (m_pScreenshotsFolderEntry)
					m_pScreenshotsFolderEntry->SetText(pszFullPath);
				if (m_pStatusLabel)
					m_pStatusLabel->SetText("Screenshots folder selected");
				break;
		}
	}
}

//-----------------------------------------------------------------------------
// Console command to open upload dialog
//-----------------------------------------------------------------------------
CON_COMMAND(cf_workshop_open_upload, "Open the Workshop upload dialog")
{
	CCFWorkshopUploadDialog* pDialog = new CCFWorkshopUploadDialog(NULL, "WorkshopUploadDialog");
	pDialog->SetupForNewItem(CF_WORKSHOP_TYPE_MAP);
	pDialog->Activate();
	pDialog->MoveToFront();
}

//-----------------------------------------------------------------------------
// Workshop Update Metadata Dialog
//-----------------------------------------------------------------------------
CCFWorkshopUpdateDialog::CCFWorkshopUpdateDialog(Panel* parent, const char* panelName)
	: BaseClass(parent, panelName)
	, m_nFileID(0)
	, m_pTitleEntry(NULL)
	, m_pDescriptionEntry(NULL)
	, m_pVisibilityCombo(NULL)
	, m_pSubmitButton(NULL)
	, m_pCancelButton(NULL)
	, m_pStatusLabel(NULL)
	, m_pProgressBar(NULL)
{
	for (int i = 0; i < CF_WORKSHOP_TAG_COUNT; i++)
	{
		m_pTagChecks[i] = NULL;
	}
	
	// Load TF2 client scheme for proper styling
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx(
		enginevgui->GetPanel(PANEL_CLIENTDLL), 
		"resource/ClientScheme.res", 
		"ClientScheme");
	SetScheme(scheme);
	SetProportional(false);
	
	SetDeleteSelfOnClose(true);
	SetSizeable(false);
	SetMoveable(true);
	SetTitle("Update Workshop Item", true);

	SetSize(600, 550);
	MoveToCenterOfScreen();
}

CCFWorkshopUpdateDialog::~CCFWorkshopUpdateDialog()
{
	// Clear the status so new dialogs can work
	if (CFWorkshop())
	{
		CFWorkshop()->ClearStatus();
	}
}

void CCFWorkshopUpdateDialog::ApplySchemeSettings(IScheme* pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	
	// Load the resource file for the update dialog
	LoadControlSettings( RES_UPDATE );
	
	// Find controls
	m_pTitleEntry = dynamic_cast<TextEntry*>(FindChildByName("TitleEntry"));
	m_pDescriptionEntry = dynamic_cast<TextEntry*>(FindChildByName("DescriptionEntry"));
	m_pVisibilityCombo = dynamic_cast<ComboBox*>(FindChildByName("VisibilityCombo"));
	m_pSubmitButton = dynamic_cast<Button*>(FindChildByName("UpdateButton"));
	m_pCancelButton = dynamic_cast<Button*>(FindChildByName("CancelButton"));
	m_pStatusLabel = dynamic_cast<Label*>(FindChildByName("StatusLabel"));
	m_pProgressBar = dynamic_cast<ProgressBar*>(FindChildByName("ProgressBar"));
	
	// Setup description entry
	if (m_pDescriptionEntry)
	{
		m_pDescriptionEntry->SetMultiline(true);
		m_pDescriptionEntry->SetCatchEnterKey(true);
		m_pDescriptionEntry->SetVerticalScrollbar(true);
	}
	
	// Setup visibility combo
	if (m_pVisibilityCombo)
	{
		m_pVisibilityCombo->RemoveAll();
		m_pVisibilityCombo->AddItem("Public", NULL);
		m_pVisibilityCombo->AddItem("Friends Only", NULL);
		m_pVisibilityCombo->AddItem("Private", NULL);
		m_pVisibilityCombo->ActivateItemByRow(0);
	}
	
	// Find tag checkboxes (same pattern as upload dialog)
	m_pTagChecks[CF_WORKSHOP_TAGIDX_MAP] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Map"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_SKIN] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Skin"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_WEAPON] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Weapon"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_GUIS] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_GUIs"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_PARTICLES] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Particles"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_SOUNDS] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Sounds"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_SPRAYS] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Sprays"));
	m_pTagChecks[CF_WORKSHOP_TAGIDX_MISC] = dynamic_cast<CheckButton*>(FindChildByName("TagCheck_Misc"));
	
	MoveToCenterOfScreen();
}

void CCFWorkshopUpdateDialog::OnCommand(const char* command)
{
	if (Q_stricmp(command, "update") == 0)
	{
		DoUpdate();
	}
	else if (Q_stricmp(command, "cancel") == 0)
	{
		Close();
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}

void CCFWorkshopUpdateDialog::OnThink()
{
	BaseClass::OnThink();
	
	// Update progress if uploading
	if (CFWorkshop() && CFWorkshop()->IsUploading())
	{
		if (m_pProgressBar)
		{
			m_pProgressBar->SetVisible(true);
			m_pProgressBar->SetProgress(CFWorkshop()->GetUploadProgress());
		}
		if (m_pStatusLabel)
			m_pStatusLabel->SetText(CFWorkshop()->GetUploadStatus());
		if (m_pSubmitButton)
			m_pSubmitButton->SetEnabled(false);
	}
	else
	{
		if (m_pSubmitButton)
			m_pSubmitButton->SetEnabled(true);
		
		const char* status = CFWorkshop() ? CFWorkshop()->GetUploadStatus() : "";
		if (status[0] != '\0')
		{
			if (m_pStatusLabel)
				m_pStatusLabel->SetText(status);
			
			// If successful, close after a moment
			if (V_strstr(status, "complete") != NULL && m_pProgressBar)
			{
				m_pProgressBar->SetVisible(true);
				m_pProgressBar->SetProgress(1.0f);
			}
		}
	}
}

void CCFWorkshopUpdateDialog::SetupForItem(PublishedFileId_t fileID)
{
	m_nFileID = fileID;
	
	char szTitle[128];
	V_snprintf(szTitle, sizeof(szTitle), "Update Workshop Item");
	SetTitle(szTitle, true);
	
	if (m_pStatusLabel)
		m_pStatusLabel->SetText("");
	if (m_pProgressBar)
	{
		m_pProgressBar->SetVisible(false);
		m_pProgressBar->SetProgress(0.0f);
	}
	
	// Fill in current values from the item
	CCFWorkshopItem* pItem = CFWorkshop() ? CFWorkshop()->GetItem(fileID) : NULL;
	if (pItem)
	{
		if (m_pTitleEntry)
			m_pTitleEntry->SetText(pItem->GetTitle());
		if (m_pDescriptionEntry)
			m_pDescriptionEntry->SetText(pItem->GetDescription());
		
		// Parse current tags and set checkboxes
		const char* pszTags = pItem->GetTags();
		if (pszTags && pszTags[0])
		{
			for (int i = 0; i < CF_WORKSHOP_TAG_COUNT; i++)
			{
				if (m_pTagChecks[i])
				{
					const char* pszTagName = g_CFWorkshopTags[i].pszTagName;
					m_pTagChecks[i]->SetSelected(V_stristr(pszTags, pszTagName) != NULL);
				}
			}
		}
	}
}

void CCFWorkshopUpdateDialog::DoUpdate()
{
	if (!CFWorkshop())
	{
		if (m_pStatusLabel)
			m_pStatusLabel->SetText("Error: Workshop system not initialized");
		return;
	}
	
	if (CFWorkshop()->IsUploading())
	{
		if (m_pStatusLabel)
			m_pStatusLabel->SetText("Update already in progress...");
		return;
	}
	
	// Get values from UI
	char szTitle[256] = {0};
	char szDescription[2048] = {0};
	
	if (m_pTitleEntry) m_pTitleEntry->GetText(szTitle, sizeof(szTitle));
	if (m_pDescriptionEntry) m_pDescriptionEntry->GetText(szDescription, sizeof(szDescription));
	
	// Validate title
	if (!szTitle[0])
	{
		if (m_pStatusLabel)
			m_pStatusLabel->SetText("Error: Title is required");
		return;
	}
	
	// Get visibility from combo
	ERemoteStoragePublishedFileVisibility visibility = k_ERemoteStoragePublishedFileVisibilityPublic;
	if (m_pVisibilityCombo)
	{
		int visRow = m_pVisibilityCombo->GetActiveItem();
		switch (visRow)
		{
			case 0: visibility = k_ERemoteStoragePublishedFileVisibilityPublic; break;
			case 1: visibility = k_ERemoteStoragePublishedFileVisibilityFriendsOnly; break;
			case 2: visibility = k_ERemoteStoragePublishedFileVisibilityPrivate; break;
		}
	}
	
	// Build tag string
	char szTags[1024] = {0};
	BuildTagString(szTags, sizeof(szTags));
	
	// Start update
	if (m_pStatusLabel)
		m_pStatusLabel->SetText("Updating item...");
	if (m_pProgressBar)
	{
		m_pProgressBar->SetVisible(true);
		m_pProgressBar->SetProgress(0.0f);
	}
	
	bool bSuccess = CFWorkshop()->StartMetadataUpdate(
		m_nFileID,
		szTitle,
		szDescription,
		visibility,
		szTags[0] ? szTags : NULL
	);
	
	if (!bSuccess && m_pStatusLabel)
	{
		m_pStatusLabel->SetText(CFWorkshop()->GetUploadStatus());
	}
}

void CCFWorkshopUpdateDialog::BuildTagString(char* pszOut, int maxLen)
{
	pszOut[0] = '\0';
	
	bool bFirst = true;
	for (int i = 0; i < CF_WORKSHOP_TAG_COUNT; i++)
	{
		if (m_pTagChecks[i] && m_pTagChecks[i]->IsSelected())
		{
			const char* pszTagName = g_CFWorkshopTags[i].pszTagName;
			
			if (bFirst)
			{
				V_strncpy(pszOut, pszTagName, maxLen);
				bFirst = false;
			}
			else
			{
				V_strncat(pszOut, ",", maxLen);
				V_strncat(pszOut, pszTagName, maxLen);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Workshop Delete Confirmation Dialog
//-----------------------------------------------------------------------------
CCFWorkshopDeleteConfirmDialog::CCFWorkshopDeleteConfirmDialog(Panel* parent, const char* panelName)
	: BaseClass(parent, panelName)
	, m_nFileID(0)
	, m_pMessageLabel(NULL)
	, m_pStatusLabel(NULL)
	, m_pDeleteButton(NULL)
	, m_pCancelButton(NULL)
	, m_bDeleteCompleted(false)
	, m_flCloseTime(0.0f)
{
	// Load TF2 client scheme for proper styling
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx(
		enginevgui->GetPanel(PANEL_CLIENTDLL), 
		"resource/ClientScheme.res", 
		"ClientScheme");
	SetScheme(scheme);
	SetProportional(false);
	
	SetDeleteSelfOnClose(true);
	SetSizeable(false);
	SetMoveable(true);
	SetTitleBarVisible(false);
	SetCloseButtonVisible(false);

	SetSize(450, 220);
	MoveToCenterOfScreen();
}

CCFWorkshopDeleteConfirmDialog::~CCFWorkshopDeleteConfirmDialog()
{
	// Clear the status so new dialogs can work
	if (CFWorkshop())
	{
		CFWorkshop()->ClearStatus();
	}
}

void CCFWorkshopDeleteConfirmDialog::ApplySchemeSettings(IScheme* pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	
	// Load the TF2-style resource file
	LoadControlSettings( RES_DELETECONFIRM );
	
	// Find controls from .res file
	m_pMessageLabel = dynamic_cast<Label*>(FindChildByName("MessageLabel"));
	m_pStatusLabel = dynamic_cast<Label*>(FindChildByName("StatusLabel"));
	m_pDeleteButton = dynamic_cast<Button*>(FindChildByName("DeleteButton"));
	m_pCancelButton = dynamic_cast<Button*>(FindChildByName("CancelButton"));
	
	MoveToCenterOfScreen();
}

void CCFWorkshopDeleteConfirmDialog::OnCommand(const char* command)
{
	if (Q_stricmp(command, "delete") == 0)
	{
		if (CFWorkshop() && m_nFileID != 0)
		{
			CFWorkshop()->DeleteItem(m_nFileID);
			if (m_pStatusLabel)
				m_pStatusLabel->SetText("Deleting...");
			if (m_pDeleteButton)
				m_pDeleteButton->SetEnabled(false);
		}
	}
	else if (Q_stricmp(command, "cancel") == 0)
	{
		Close();
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}

void CCFWorkshopDeleteConfirmDialog::OnThink()
{
	BaseClass::OnThink();
	
	// If we've completed delete and set a close time, check if it's time to close
	if (m_bDeleteCompleted && m_flCloseTime > 0.0f)
	{
		if (gpGlobals->curtime >= m_flCloseTime)
		{
			Close();
			return;
		}
	}
	
	// Check delete status
	if (CFWorkshop() && !m_bDeleteCompleted)
	{
		const char* status = CFWorkshop()->GetUploadStatus();
		if (status[0] != '\0' && m_pStatusLabel)
		{
			m_pStatusLabel->SetText(status);
			
			// If delete completed, set up delayed close
			if (V_strstr(status, "deleted") != NULL)
			{
				m_bDeleteCompleted = true;
				m_flCloseTime = gpGlobals->curtime + 1.5f; // Close after 1.5 seconds
				
				// Clear the status so other dialogs can work
				// (The manager will have already processed the delete)
			}
		}
		
		// Re-enable button if delete is not in progress and not completed
		if (!CFWorkshop()->IsDeleteInProgress() && m_pDeleteButton && !m_bDeleteCompleted)
		{
			m_pDeleteButton->SetEnabled(true);
		}
	}
}

void CCFWorkshopDeleteConfirmDialog::SetupForItem(PublishedFileId_t fileID, const char* pszTitle)
{
	m_nFileID = fileID;
	
	if (m_pMessageLabel)
	{
		char szMessage[512];
		V_snprintf(szMessage, sizeof(szMessage), 
			"Are you sure you want to permanently delete '%s' from the Steam Workshop?\n\nThis action cannot be undone.",
			pszTitle);
		m_pMessageLabel->SetText(szMessage);
	}
	
	if (m_pStatusLabel)
		m_pStatusLabel->SetText("");
}
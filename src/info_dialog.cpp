#include "tattle.h"

#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/statline.h>
#include <wx/sizer.h>
#include <wx/artprov.h>
#include <wx/statbmp.h>
#include <wx/hyperlink.h>

using namespace tattle;

wxBEGIN_EVENT_TABLE(InfoDialog, wxDialog)
	EVT_BUTTON(wxID_OK, InfoDialog::OnOk)
	EVT_BUTTON(wxID_OPEN, InfoDialog::OnOpen)
	EVT_BUTTON(wxID_CANCEL, InfoDialog::OnCancel)
	EVT_BUTTON(wxID_CLOSE, InfoDialog::OnCancel)
	EVT_HYPERLINK(wxID_OPEN, InfoDialog::OnLink)
	EVT_CLOSE(InfoDialog::OnClose)
wxEND_EVENT_TABLE()

InfoDialog::InfoDialog(wxWindow *parent, wxString title, wxString message, wxString link_, Report::SERVER_COMMAND _command, wxArtID iconArtID) :
	wxDialog(parent, -1, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | uiConfig.style()),
	command(_command), link(link_)
{
	// | wxOK | wxCENTER | (link_.length() ? wxCANCEL : 0)

	//wxMessageBox(message, title, wxOK | wxCENTER | (link_.length() ? wxCANCEL : 0));

	didAction = false;
	overrideCommand = false;

#ifdef __WXMAC__
	wxSize badgeSize(16, 16);
#else
	wxSize badgeSize(32, 32);
#endif

	wxIcon iconInfo = wxArtProvider::GetIcon(iconArtID.length() ? iconArtID : uiConfig.defaultIcon());
	wxBitmap bmpInfo = wxArtProvider::GetBitmap(iconArtID.length() ? iconArtID : uiConfig.defaultIcon(), "wxART_OTHER_C", badgeSize);

	SetIcon(iconInfo);

	wxBoxSizer *sizerTop = new wxBoxSizer(wxVERTICAL);

	{
		wxBoxSizer *display = new wxBoxSizer(wxHORIZONTAL);

		display->Add(new wxStaticBitmap(this, -1, bmpInfo), 0, wxTOP|wxLEFT, uiConfig.marginLg());

		display->AddSpacer(uiConfig.marginSm());

		{
			wxBoxSizer *textArea = new wxBoxSizer(wxVERTICAL);

			textArea->AddSpacer(uiConfig.marginSm());

			message.Replace("\r", "");

			wxString header, content;

			{
				wxString::size_type pos = message.find("\n\n");
				if (pos == wxString::npos)
				{
					content = message;
				}
				else
				{
					header = message.substr(0, pos);
					content = message.substr(pos+2);
				}
			}

			wxStaticText
				*tHeader = (header.length() ? new wxStaticText(this, -1, header) : NULL),
				*tContent = new wxStaticText(this, -1, content);

			if (tHeader)
			{
				tHeader->SetFont(wxFontInfo(12).AntiAliased()); //Family(wxFONTFAMILY_SWISS)
				tHeader->SetForegroundColour(wxTheColourDatabase->Find("MEDIUM BLUE"));
				textArea->Add(tHeader, 0, wxALIGN_LEFT | wxALL, uiConfig.marginMd());
			}
			textArea->Add(tContent, 0, wxALIGN_LEFT | wxALL, uiConfig.marginMd());

			if (link.length())
			{
				wxString shortLink = link;
				{
					wxString::size_type pos = link.find("//");
					if (pos != wxString::npos) shortLink = link.substr(pos + 2);
				}

				wxHyperlinkCtrl *anchor = new wxHyperlinkCtrl(this, wxID_OPEN, shortLink, link);

				textArea->Add(anchor, 0, wxALIGN_RIGHT | wxALL, uiConfig.marginMd());
			}

			display->Add(textArea, 0);
		}

		sizerTop->Add(display, 0);
	}

	{
		wxBoxSizer *actionRow = new wxBoxSizer(wxHORIZONTAL);

		wxButton *dfl = NULL;

		if (link.length())
		{
			actionRow->Add(dfl = new wxButton(this, wxID_OPEN), 1, wxALL, uiConfig.marginSm());
			actionRow->Add(new wxButton(this, wxID_CANCEL), 0, wxALL, uiConfig.marginSm());
		}
		else
		{
			actionRow->Add(dfl = new wxButton(this, wxID_OK), 1, wxALL, uiConfig.marginSm());
		}

		if (dfl)
		{
			dfl->SetDefault();
			dfl->SetFocus();
		}

		sizerTop->Add(actionRow, 0, wxALIGN_RIGHT | wxALL, uiConfig.marginSm());
	}

	SetSizerAndFit(sizerTop);

	Center(wxBOTH);
}

void InfoDialog::Done()
{
	// If we destroy this window AFTER spawning the prompt under wxMSW,
	//    the prompt disappears and cannot be recovered.
	Disable();
	
	void(*action)(void) = NULL;

	if (!overrideCommand)
		switch (command)
	{
	default:
	case Report::SC_NONE:
		action = &Tattle_Proceed;
		break;
	case Report::SC_PROMPT:
		action = &Tattle_ShowPrompt;
		break;
	case Report::SC_STOP:
		action = &Tattle_Halt;
		break;
	case Report::SC_STOP_ON_LINK:
		action = (didAction ? &Tattle_Halt : &Tattle_Proceed);
		break;
	}

	Destroy();
	if (action) (*action)();
}

void InfoDialog::OpenLink()
{
	if (wxLaunchDefaultBrowser(link))
	{
		didAction = true;
	}
	else if (wxLaunchDefaultApplication(link))
	{
		didAction = true;
	}
	else
	{
		// Notify about failure -- TEST THIS
		Tattle_InsertDialog(new InfoDialog(GetParent(),
			wxT("Failed to open link"), wxT("The link couldn't be opened for some reason:\n[") + link + wxT("]")));
		overrideCommand = true;
	}
}

void InfoDialog::OnLink(wxHyperlinkEvent &evt)
{
	OpenLink();
	Done();
}

void InfoDialog::OnOpen(wxCommandEvent &evt)
{
	OpenLink();
	Done();
}

void InfoDialog::OnOk(wxCommandEvent &evt) { Done(); }

void InfoDialog::OnCancel(wxCommandEvent &evt) { Done(); }

void InfoDialog::OnClose(wxCloseEvent &evt)    { Done(); }

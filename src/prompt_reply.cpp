#include "tattle.h"

#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/statline.h>
#include <wx/sizer.h>

using namespace tattle;

wxBEGIN_EVENT_TABLE(InfoDialog, wxDialog)
	EVT_BUTTON(wxID_OK, InfoDialog::OnOk)
	EVT_BUTTON(wxID_OPEN, InfoDialog::OnOpen)
	EVT_BUTTON(wxID_CANCEL, InfoDialog::OnCancel)
	EVT_CLOSE(InfoDialog::OnClose)
wxEND_EVENT_TABLE()

InfoDialog::InfoDialog(wxWindow *parent, wxString title, wxString message, wxString link_, Report::SERVER_COMMAND _command) :
	wxDialog(parent, -1, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | uiConfig.style()),
	command(_command), link(link_)
{
	// | wxOK | wxCENTER | (link_.length() ? wxCANCEL : 0)

	wxMessageBox(message, title, wxOK | wxCENTER | (link_.length() ? wxCANCEL : 0));

	didAction = false;
	overrideCommand = false;

	wxBoxSizer *sizerTop = new wxBoxSizer(wxVERTICAL);

	sizerTop->AddSpacer(uiConfig.margin);

	{
		wxStaticText *messageText = new wxStaticText(this, -1, message,
			wxDefaultPosition, wxDefaultSize, wxLEFT);

		sizerTop->Add(messageText, 1, wxALIGN_LEFT | wxALL, uiConfig.margin);
	}

	sizerTop->AddSpacer(uiConfig.margin);

	{
		wxBoxSizer *actionRow = new wxBoxSizer(wxHORIZONTAL);

		wxButton *dfl = NULL;

		if (link.length())
		{
			actionRow->Add(dfl = new wxButton(this, wxID_OPEN), 1, wxALL, uiConfig.margin);
			actionRow->Add(new wxButton(this, wxID_CANCEL), 0, wxALL, uiConfig.margin);
		}
		else
		{
			actionRow->Add(dfl = new wxButton(this, wxID_OK), 1, wxALL, uiConfig.margin);
		}

		if (dfl) dfl->SetDefault();

		sizerTop->Add(actionRow, 0, wxALIGN_CENTER | wxALL);
	}

	sizerTop->AddSpacer(uiConfig.margin);

	SetSizerAndFit(sizerTop);
}

void InfoDialog::Done()
{
	if (!overrideCommand)
		switch (command)
	{
	default:
	case Report::SC_PROMPT:
		Tattle_Proceed();
		break;
	case Report::SC_STOP:
		Tattle_Halt();
		break;
	case Report::SC_STOP_ON_LINK:
		if (didAction) Tattle_Halt();
		else           Tattle_Proceed();
		break;
	}
	Destroy();
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

void InfoDialog::OnOpen(wxCommandEvent &evt)
{
	OpenLink();
	Done();
}

void InfoDialog::OnOk(wxCommandEvent &evt) { Done(); }

void InfoDialog::OnCancel(wxCommandEvent &evt) { Done(); }

void InfoDialog::OnClose(wxCloseEvent &evt)    { Done(); }

wxWindow *Prompt::DisplayReply(const Report::Reply &reply, wxWindow *parent)
{
	wxString errorMessage;

	bool didAction = false;

	if (!reply.connected)
	{
		errorMessage =
			"Failed to reach the website for the report.\nAre you connected to the internet?";
	}
	else if (!reply.ok())
	{
		errorMessage = "Failed to send the report.\n";

		if (reply.statusCode == 404)
		{
			errorMessage += "(server script not found)";
		}
		else
		{
			errorMessage += wxString::Format(wxT("HTTP status %i / WXE #%i"),
				int(reply.statusCode), int(reply.error));
		}
	}
	else if (!reply.valid())
	{
		// Reply is not formatted for tattle... assume
		errorMessage = wxT("The report went to the wrong place.");

		if (reply.raw.Length())
		{
			wxString htmlTitle = GetTagContents(reply.raw, wxT("title"));
			if (htmlTitle.Length())
			{
				if (htmlTitle.Length() > 128) htmlTitle.Truncate(128);
				errorMessage += "\n\nGot a page titled: `" + htmlTitle + "'";
			}
			else
			{
				errorMessage += wxT("\nAre you connected to the internet?");

				wxString raw = reply.raw;
				if (raw.Length() > 512)
				{
					raw.Truncate(512);
					raw += wxT(" ...");
				}
				errorMessage += wxT("\n\nThe server says:\n") + raw;
			}
		}
		else
		{
			errorMessage += wxT("\nAre you connected to the internet?");
		}
	}
	else if (reply.sentLink())
	{
		wxString title = reply.title, msg = reply.message;

		if (!title.Length()) title = wxT("Suggested Link");
		if (!msg.Length()) msg = wxT("The server replied with a link.");

		msg += wxT("\n\n") + reply.link + wxT("\nOpen in your browser?");

		return new InfoDialog(parent, title, msg, reply.link);
	}
	else if (reply.message.Length() > 0)
	{
		wxString title = reply.title, msg = reply.message;
		if (!title.Length()) title = "Report Sent";
		if (!msg.Length()) msg = "Your report was sent and accepted.";

		return new InfoDialog(parent, title, msg);
	}

	if (errorMessage.Length())
	{
		return new InfoDialog(parent, wxT("Send Failed"), errorMessage);
	}

	return NULL;
}
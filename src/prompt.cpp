//
//  prompt.cpp
//  tattle
//
//  Created by Evan Balster on 11/12/16.
//

#include "tattle.h"

//Dialog stuff
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/statline.h>
#include <wx/sizer.h>
#include <wx/msw/winundef.h>


using namespace tattle;


wxBEGIN_EVENT_TABLE(Prompt, wxDialog)
	EVT_BUTTON(Prompt::Ev_Submit,  Prompt::OnSubmit)
	EVT_BUTTON(Prompt::Ev_Cancel,  Prompt::OnCancel)
	EVT_BUTTON(Prompt::Ev_Details, Prompt::OnDetails)
	EVT_CLOSE (Prompt::OnClose)
	EVT_SHOW  (Prompt::OnShow)
wxEND_EVENT_TABLE()

void Prompt::OnDetails(wxCommandEvent & event)
{
	if (!ViewReport::Exists())
	{
		ViewReport *viewReport = new ViewReport(this, -1);
		
		viewReport->Show();
	}
}

void Prompt::OnSubmit(wxCommandEvent & event)
{
	std::string input_warnings;

	for (Fields::iterator i = fields.begin(); i != fields.end(); ++i)
	{
		if (i->content->input_warning().length() && !i->control->GetValue().length())
		{
			input_warnings += i->content->input_warning();
			input_warnings += "\n";
		}
	}

	if (input_warnings.length())
	{
		if (wxMessageBox(input_warnings + "\nSubmit the report anyway?",
			"Missing Information", wxYES_NO|wxCENTRE, this)
			!= wxYES)
			return;
	}

	// Halt user input
	Enable(false);

	// Upload user field values
	UpdateReportFromFields();

	// Proceed to the posting stage
	Tattle_Proceed();
}

void Prompt::OnCancel(wxCommandEvent & event)
{
	Close(true);
}

void Prompt::OnShow(wxShowEvent & event)
{
}

void Prompt::OnClose(wxCloseEvent &event)
{
	auto id = report.identity();

	if (dontShowAgainBox && dontShowAgainBox->GetValue())
		persist.mergePatch({{"$show", { {id.type, {{id.id, 0}} }} }});

	// TODO consider veto appeal
	Enable(false);

	Tattle_Halt();
}

void Prompt::UpdateReportFromFields()
{
	auto id = report.identity();

	Json persistInputs = Json::object();

	for (Fields::iterator i = fields.begin(); i != fields.end(); ++i)
	{
		if (i->content->persist())
		{
			persistInputs[i->content->name] = i->control->GetValue();
		}

		i->content->user_input = i->control->GetValue();
	}

	if (dontShowAgainBox && dontShowAgainBox->GetValue())
	{
		persistInputs["$show"] = {{id.type, {{id.id, 0}} }};
	}

	if (persistInputs.size())
		persist.mergePatch(persistInputs);
}

static void ApplyMarkup(wxControl *control, const wxString &markup)
{
	wxString processed = markup;
	processed.Replace(_("<br>"), _("\n"), true);
	control->SetLabelMarkup(processed);
}

wxWindow *Prompt::DisplayReply(const Report::Reply &reply, wxWindow *parent)
{
	wxString errorMessage;

	if (!reply.connected())
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
			switch (reply.requestState)
			{
			case wxWebRequest::State_Idle: errorMessage += "Request was not initiated (Idle)"; break;
			case wxWebRequest::State_Active: errorMessage += "Request was not finished (Active)"; break;
			case wxWebRequest::State_Cancelled: errorMessage += "Request was cancelled"; break;
			case wxWebRequest::State_Failed:
				if (reply.statusCode)
					errorMessage += wxString::Format(wxT("Request failed (status %d)"), int(reply.statusCode));
				else
					errorMessage += "Request failed (no connection to server)";
				break;
			case wxWebRequest::State_Unauthorized:
				errorMessage += wxString::Format(wxT("Request unauthorized (status %d)"), int(reply.statusCode));
				break;
			case wxWebRequest::State_Completed:
				errorMessage += wxString::Format(wxT("Request completed with status %d"), int(reply.statusCode));
				break;
			}
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
	else
	{
		wxString title = reply.title, msg = reply.message;
		wxString link;
		
		if (reply.sentLink())
		{
			if (!title.Length()) title = wxT("Suggested Link");
			if (!msg.Length()) msg = wxT("The server replied with a link.");
			link = reply.link;
		}
		else
		{
			if (!title.Length()) title = "Report Sent";
			if (!msg.Length()) msg = "Your report was sent and accepted.";
		}

		if (reply.identity && persist.shouldShow(reply.identity))
		{
			return new InfoDialog(parent, title, msg, reply.link, reply.command, reply.icon, reply.identity);
		}
		else
		{
			switch (reply.command)
			{
			default:
			case Report::SC_NONE:
			case Report::SC_PROMPT:
			case Report::SC_STOP_ON_LINK: // (user has only dismissed server message)
				// Proceed to prompt, if any.
				break;
			case Report::SC_STOP:
				Tattle_Halt();
				break;
			}
			return NULL; // If we previously chose "don't show this again"...
		}
	}

	if (errorMessage.Length())
	{
		return new InfoDialog(parent, wxT("Send Failed"), errorMessage, "", Report::SC_PROMPT, reply.icon, reply.identity);
	}

	return NULL;
}

// Dialog
Prompt::Prompt(wxWindow * parent, wxWindowID id, Report &_report)
	: wxDialog(parent, id, _report.url_post().host + ": " + uiConfig.promptTitle(),
		wxDefaultPosition, wxDefaultSize,
		wxDEFAULT_DIALOG_STYLE | uiConfig.style()),
	report(_report),
	fontTechnical(8, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL)
{
	SetIcon(wxArtProvider::GetIcon(uiConfig.defaultIcon()));


	const unsigned MARGIN = uiConfig.marginSm();

	// Error display ?
	//wxTextCtrl *displayError 

	wxBoxSizer *sizerTop = new wxBoxSizer(wxVERTICAL);
	
	sizerTop->AddSpacer(MARGIN);

	if (uiConfig.promptMessage().length())
	{
		wxStaticText *messageText = new wxStaticText(this, -1, uiConfig.promptMessage(),
			wxDefaultPosition, wxDefaultSize, wxLEFT);
		
		ApplyMarkup(messageText, uiConfig.promptMessage());
		
		sizerTop->Add(messageText, 0, wxALIGN_LEFT | wxALL, MARGIN);

		// Horizontal rule, if no technical message
		if (uiConfig.promptTechnical().length() == 0)
			sizerTop->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, MARGIN);
	}
	
	// Report contents and information
	{
		// Data area
		wxTextCtrl *techBox = nullptr;
		dontShowAgainBox = nullptr;
		wxButton *reviewButton = nullptr;

		if (report.identity())
		{
			dontShowAgainBox = new wxCheckBox(this, -1, "Don't show again");
		}

		if (uiConfig.enableReview())
		{
			reviewButton = new wxButton(this, Ev_Details, uiConfig.labelReview());
		}

		if (uiConfig.promptTechnical().length())
		{
			wxString technical = uiConfig.promptTechnical();
			technical.Replace(_("<br>"), _("\n"), true);

			techBox = new wxTextCtrl(this, -1, technical,
				wxDefaultPosition, wxSize(300, 40), // previously 400
				wxTE_MULTILINE|wxTE_READONLY, wxDefaultValidator);

			techBox->SetFont(fontTechnical);

			techBox->SetBackgroundStyle( wxBG_STYLE_COLOUR );
			techBox->SetBackgroundColour( *wxLIGHT_GREY );

			//sizerTop->Add(techBox, 0, wxEXPAND | wxALL, MARGIN);
		}

		wxBoxSizer *myDataControls = nullptr;

		if (dontShowAgainBox || reviewButton)
		{
			myDataControls = new wxBoxSizer(wxVERTICAL);

			if (reviewButton)     myDataControls->Add(reviewButton,     0, wxALIGN_CENTER | wxALL, MARGIN);
			if (dontShowAgainBox) myDataControls->Add(dontShowAgainBox, 0, wxALL);
		}

		if (techBox)
		{
			if (myDataControls)
			{
				wxBoxSizer *myDataHori = new wxBoxSizer(wxHORIZONTAL);
				myDataHori->Add(techBox, 0, wxEXPAND | wxALL, MARGIN);
				myDataHori->Add(myDataControls, 0, wxEXPAND | wxALL, MARGIN);
				sizerTop->Add(myDataHori, 0, wxEXPAND | wxALL);
			}
			else
			{
				sizerTop->Add(techBox, 0, wxEXPAND | wxALL, MARGIN);
			}
		}
		else if (myDataControls)
		{
			sizerTop->Add(myDataControls, 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT, MARGIN);
		}
	}
	

	wxFlexGridSizer *sizerField = NULL;
	
	wxTextCtrl *firstField = NULL;

	for (Report::Contents::const_iterator i = report.contents().begin(); i != report.contents().end(); ++i)
		switch (i->type)
	{
	case PARAM_FIELD:
		{
			wxStaticText *label = new wxStaticText(this, -1, i->label());
			
			ApplyMarkup(label, i->label());

			// Initial field value may come from persistent data store.
			wxString initialFieldValue = i->value();
			if (i->persist()) // TODO potential encoding problems here??
			{
				if (persist.data.contains(i->name))
					initialFieldValue = persist.data.value(i->name, std::string(initialFieldValue));
			}

			wxTextCtrl *field = new wxTextCtrl(this, -1, initialFieldValue,
				wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, i->name);
				
			Field fieldEntry = {&*i, field}; fields.push_back(fieldEntry);

			if (i->placeholder().length()) field->SetHint(i->placeholder());

			if (!sizerField)
			{
				// Start a new flex grid for this run of one-liner fields
				sizerField = new wxFlexGridSizer(2);
				sizerField->AddGrowableCol(1, 1);
				sizerTop->Add(sizerField, 0, wxEXPAND | wxALL, 0);
			}

			sizerField->Add(label, 0, wxALL | wxALIGN_CENTER_VERTICAL, MARGIN);
			sizerField->Add(field, 0, wxEXPAND | wxALL, MARGIN);
			
			if (!firstField) firstField = field;
		}
		break;

	case PARAM_FIELD_MULTI:
		{
			if (sizerField) sizerField = NULL;

			wxStaticText *label = new wxStaticText(this, -1, i->label());

			wxTextCtrl *field = new wxTextCtrl(this, -1, i->value(),
				wxDefaultPosition, wxSize(400, 100), wxTE_MULTILINE, wxDefaultValidator, i->name);
				
			Field fieldEntry = {&*i, field}; fields.push_back(fieldEntry);

			sizerTop->Add(label, 0, wxEXPAND | wxALL, MARGIN);
			sizerTop->Add(field, 0, wxEXPAND | wxALL, MARGIN);
			
			if (!firstField) firstField = field;
		}
		
	default:
		// Not fields
		break;
	}

	if (report.connectionWarning)
	{
		wxStaticText *messageText = new wxStaticText(this, -1,
			wxT("Check your internet connection."));

		messageText->SetForegroundColour(*wxRED);

		sizerTop->Add(messageText, 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT, MARGIN);
	}

	{
		wxBoxSizer *actionRow = new wxBoxSizer(wxHORIZONTAL);

		wxButton *butSubmit, *butCancel;
		
		butSubmit = new wxButton(this, Ev_Submit, uiConfig.labelSend());
		butCancel = new wxButton(this, Ev_Cancel, uiConfig.labelCancel());

		{
			wxStaticText *actionsLabel = new wxStaticText(this, -1, uiConfig.promptMessage(),
				wxDefaultPosition, wxDefaultSize, wxLEFT);

			ApplyMarkup(actionsLabel, uiConfig.labelPrompt());
			actionRow->Add(actionsLabel, 1, wxALIGN_CENTER | wxALL, MARGIN);
		}

		actionRow->Add(butSubmit, 1, wxALL, MARGIN);
		actionRow->Add(butCancel, 0, wxALL, MARGIN);
		
		butSubmit->SetDefault();

		sizerTop->Add(actionRow, 0, wxALIGN_RIGHT | wxALL);
	}
	
	sizerTop->AddSpacer(MARGIN);

	// Apply the sizing scheme
	SetSizerAndFit(sizerTop);

	Center(wxBOTH);
	
	if (firstField) firstField->SetFocus();
}

Prompt::~Prompt()
{
	int i = 5;
}


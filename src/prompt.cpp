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

void Prompt::OnClose(wxCloseEvent &event)
{
	// TODO consider veto appeal
	Enable(false);

	Tattle_Halt();
}

void Prompt::OnShow(wxShowEvent & event)
{
}

void Prompt::UpdateReportFromFields()
{
	for (Fields::iterator i = fields.begin(); i != fields.end(); ++i)
	{
		i->param->value = i->control->GetValue();
	}
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

		return new InfoDialog(parent, title, msg, reply.link, reply.command, reply.icon);
	}
	else if (reply.message.Length() > 0)
	{
		wxString title = reply.title, msg = reply.message;
		if (!title.Length()) title = "Report Sent";
		if (!msg.Length()) msg = "Your report was sent and accepted.";

		return new InfoDialog(parent, title, msg, "", reply.command, reply.icon);
	}

	if (errorMessage.Length())
	{
		return new InfoDialog(parent, wxT("Send Failed"), errorMessage, "", Report::SC_PROMPT, reply.icon);
	}

	return NULL;
}

// Dialog
Prompt::Prompt(wxWindow * parent, wxWindowID id, Report &_report)
	: wxDialog(parent, id, uiConfig.promptTitle,
		wxDefaultPosition, wxDefaultSize,
		wxDEFAULT_DIALOG_STYLE | uiConfig.style()),
	report(_report),
	fontTechnical(wxFontInfo().Family(wxFONTFAMILY_TELETYPE))
{
	SetIcon(wxArtProvider::GetIcon(uiConfig.defaultIcon));


	const unsigned MARGIN = uiConfig.marginSm;

	// Error display ?
	//wxTextCtrl *displayError 

	wxBoxSizer *sizerTop = new wxBoxSizer(wxVERTICAL);
	
	sizerTop->AddSpacer(MARGIN);

	if (uiConfig.promptMessage.length())
	{
		wxStaticText *messageText = new wxStaticText(this, -1, uiConfig.promptMessage,
			wxDefaultPosition, wxDefaultSize, wxLEFT);
		
		ApplyMarkup(messageText, uiConfig.promptMessage);
		
		sizerTop->Add(messageText, 0, wxALIGN_LEFT | wxALL, MARGIN);

		// Horizontal rule, if no technical message
		if (uiConfig.promptTechnical.length() == 0)
			sizerTop->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, MARGIN);
	}
	
	if (uiConfig.promptTechnical.length())
	{
		wxString technical = uiConfig.promptTechnical;
		technical.Replace(_("<br>"), _("\n"), true);
	
		wxTextCtrl *techBox = new wxTextCtrl(this, -1, technical,
			wxDefaultPosition, wxSize(400, 40),
			wxTE_MULTILINE|wxTE_READONLY, wxDefaultValidator);
		
		techBox->SetFont(fontTechnical);
		
		techBox->SetBackgroundStyle( wxBG_STYLE_COLOUR );
        techBox->SetBackgroundColour( *wxLIGHT_GREY );
		
		sizerTop->Add(techBox, 0, wxEXPAND | wxALL, MARGIN);
	}

	wxFlexGridSizer *sizerField = NULL;
	
	wxTextCtrl *firstField = NULL;

	for (Report::Parameters::iterator i = report.params.begin(); i != report.params.end(); ++i)
		switch (i->type)
	{
	case PARAM_FIELD:
		{
			wxStaticText *label = new wxStaticText(this, -1, i->label);
			
			ApplyMarkup(label, i->label);

			wxTextCtrl *field = new wxTextCtrl(this, -1, i->value,
				wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, i->name);
				
			Field fieldEntry = {&*i, field}; fields.push_back(fieldEntry);

			if (i->hint.length()) field->SetHint(i->hint);

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

			wxStaticText *label = new wxStaticText(this, -1, i->label);

			wxTextCtrl *field = new wxTextCtrl(this, -1, i->value,
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
		
		butSubmit = new wxButton(this, Ev_Submit, uiConfig.labelSend);
		butCancel = new wxButton(this, Ev_Cancel, uiConfig.labelCancel);

		actionRow->Add(butSubmit, 1, wxALL, MARGIN);
		actionRow->Add(butCancel, 0, wxALL, MARGIN);
		
		if (uiConfig.viewEnabled)
		{
			wxButton *butView = new wxButton(this, Ev_Details, uiConfig.labelView);
			actionRow->Add(butView, 0, wxALL, MARGIN);
		}
		
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


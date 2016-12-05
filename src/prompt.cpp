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
wxEND_EVENT_TABLE()

void Prompt::OnDetails(wxCommandEvent & event)
{
	if (!ViewReport::Exists())
	{
		ViewReport *viewReport = new ViewReport(this, -1, report);
		
		viewReport->Show();
	}
}

bool Prompt::DisplayReply(const Report::Reply &reply, wxWindow *parent, bool stayOnTop)
{
	wxString errorMessage;
	
	int stayOnTopFlag = (stayOnTop ? wxSTAY_ON_TOP : 0);
	
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
			errorMessage += "(uploader script not found)";
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
		if (!msg  .Length()) msg   = wxT("The server replied with a link.");
		
		msg += wxT("\n\n") + reply.link + wxT("\nOpen in your browser?");
		
		wxMessageDialog *offerLink = new wxMessageDialog(parent,
			msg, title, wxICON_INFORMATION|wxOK|wxCANCEL|wxCENTER | stayOnTopFlag);
		
		offerLink->SetOKLabel(wxT("Open Link"));
		
		const int result = offerLink->ShowModal();
		
		delete offerLink;
		
		if (result == wxID_OK) 
		{
			if (wxLaunchDefaultBrowser(reply.link))
			{
				didAction = true;
			}
			else
			{
				wxMessageBox(wxT("The link couldn't be opened for some reason:\n[")+reply.link+wxT("]"), wxT("Failed to open link"),
					wxOK|wxCENTER | stayOnTopFlag, parent);
				wxLaunchDefaultApplication(reply.link);
			}
		} 
	}
	else if (reply.message.Length() > 0)
	{
		wxString title = reply.title, msg = reply.message;
		if (!title.Length()) title = "Report Sent";
		if (!msg  .Length()) msg   = "Your report was sent and accepted.";
		
		wxMessageBox(msg, title, wxOK|wxCENTER | stayOnTopFlag, parent);
	}
	
	if (errorMessage.Length())
	{
		wxMessageBox(errorMessage, wxT("Send Failed"),
			wxICON_ERROR|wxOK|wxCENTER | stayOnTopFlag, parent);
	}
	
	return didAction;
}

void Prompt::OnSubmit(wxCommandEvent & event)
{
	UpdateReportFromFields();
	
	Report::Reply reply = report.httpPost();
	
	if (report.stayOnTop) Show(0);
	
	DisplayReply(reply, this, report.stayOnTop);
	
	if (report.stayOnTop) Show(1);
	
	if (reply.valid()) Close();
}

void Prompt::OnCancel(wxCommandEvent & event)
{
	//wxMessageBox(_("User canceled the report."));
	Close(true);
}

void Prompt::UpdateReportFromFields()
{
	for (Fields::iterator i = fields.begin(); i != fields.end(); ++i)
	{
		i->param->value = i->control->GetValue();
	}
}

static const unsigned MARGIN = 5;

static void ApplyMarkup(wxControl *control, const wxString &markup)
{
	wxString processed = markup;
	processed.Replace(_("<br>"), _("\n"), true);
	control->SetLabelMarkup(processed);
}

// Dialog
Prompt::Prompt(wxWindow * parent, wxWindowID id, Report &_report)
	: wxDialog(parent, id, _report.promptTitle,
		wxDefaultPosition, wxDefaultSize,
		wxDEFAULT_DIALOG_STYLE | (_report.stayOnTop ? wxSTAY_ON_TOP : 0)),
	report(_report),
	fontTechnical(wxFontInfo().Family(wxFONTFAMILY_TELETYPE))
{
	// Error display ?
	//wxTextCtrl *displayError 

	wxBoxSizer *sizerTop = new wxBoxSizer(wxVERTICAL);
	
	sizerTop->AddSpacer(MARGIN);

	if (report.promptMessage.length())
	{
		wxStaticText *messageText = new wxStaticText(this, -1, report.promptMessage,
			wxDefaultPosition, wxDefaultSize, wxLEFT);
		
		ApplyMarkup(messageText, report.promptMessage);
		
		sizerTop->Add(messageText, 0, wxALIGN_LEFT | wxALL, MARGIN);

		// Horizontal rule, if no technical message
		if (report.promptTechnical.length() == 0)
			sizerTop->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, MARGIN);
	}
	
	if (report.promptTechnical.length())
	{
		wxString technical = report.promptTechnical;
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
		
		sizerTop->Add(messageText, 0, wxALIGN_CENTER | wxALL, MARGIN);
	}

	{
		wxBoxSizer *actionRow = new wxBoxSizer(wxHORIZONTAL);

		wxButton *butSubmit, *butCancel;
		
		butSubmit = new wxButton(this, wxID_OK, report.labelSend);
		butCancel = new wxButton(this, wxID_CANCEL, report.labelCancel);

		actionRow->Add(butSubmit, 1, wxEXPAND | wxALL, MARGIN);
		actionRow->Add(butCancel, 0, wxALL, MARGIN);
		
		if (report.viewEnabled)
		{
			wxButton *butView = new wxButton(this, wxID_VIEW_DETAILS, report.labelView);
			actionRow->Add(butView, 0, wxALL, MARGIN);
		}
		
		butSubmit->SetDefault();

		sizerTop->Add(actionRow, 0, wxALIGN_CENTER | wxALL);
	}
	
	sizerTop->AddSpacer(MARGIN);

	// Apply the sizing scheme
	SetSizerAndFit(sizerTop);
	
	if (firstField) firstField->SetFocus();
}

void Prompt::OnClose(wxCloseEvent &event)
{
	// TODO consider veto
	Destroy();
}

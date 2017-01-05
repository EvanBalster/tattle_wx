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
	
	Report::Reply reply = report.httpPost(this);
	
	if (reply.valid())
	{
		// Show reply dialog and destroy the prompt
		Tattle_InsertDialog(DisplayReply(reply, this));
		Destroy();
	}
	else
	{
		// ...Should really insert an error message here, but it doesn't jive with the workflow
	}
}

void Prompt::OnCancel(wxCommandEvent & event)
{
	Close(true);
}

void Prompt::OnClose(wxCloseEvent &event)
{
	// TODO consider veto appeal

	Tattle_Halt();

	Destroy();
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

// Dialog
Prompt::Prompt(wxWindow * parent, wxWindowID id, Report &_report)
	: wxDialog(parent, id, uiConfig.promptTitle,
		wxDefaultPosition, wxDefaultSize,
		wxDEFAULT_DIALOG_STYLE | uiConfig.style()),
	report(_report),
	fontTechnical(wxFontInfo().Family(wxFONTFAMILY_TELETYPE))
{
	const unsigned MARGIN = uiConfig.margin;

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
		
		sizerTop->Add(messageText, 0, wxALIGN_CENTER | wxALL, MARGIN);
	}

	{
		wxBoxSizer *actionRow = new wxBoxSizer(wxHORIZONTAL);

		wxButton *butSubmit, *butCancel;
		
		butSubmit = new wxButton(this, wxID_OK, uiConfig.labelSend);
		butCancel = new wxButton(this, wxID_CANCEL, uiConfig.labelCancel);

		actionRow->Add(butSubmit, 1, wxEXPAND | wxALL, MARGIN);
		actionRow->Add(butCancel, 0, wxALL, MARGIN);
		
		if (uiConfig.viewEnabled)
		{
			wxButton *butView = new wxButton(this, wxID_VIEW_DETAILS, uiConfig.labelView);
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

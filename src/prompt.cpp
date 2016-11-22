//
//  prompt.cpp
//  tattle
//
//  Created by Evan Balster on 11/12/16.
//  Copyright © 2016 imitone. All rights reserved.
//

#include "tattle.h"


//Dialog
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/statline.h>
#include <wx/sizer.h>

//HTML encoding
#include <wx/sstream.h>


using namespace tattle;


wxBEGIN_EVENT_TABLE(Prompt, wxDialog)
	EVT_BUTTON(Prompt::Ev_Submit,  Prompt::OnSubmit)
	EVT_BUTTON(Prompt::Ev_Cancel,  Prompt::OnCancel)
	EVT_BUTTON(Prompt::Ev_Details, Prompt::OnDetails)
	EVT_CLOSE (Prompt::OnClose)
wxEND_EVENT_TABLE()

void Prompt::OnDetails(wxCommandEvent & event)
{
	wxMessageBox(_("User requested details."));
}

void Prompt::OnSubmit(wxCommandEvent & event)
{
	UpdateReportFromFields();

	wxHTTP post;
	report.encodePost(post);
	
	post.SetTimeout(10);
	
	//wxString 
	
	if (!post.Connect(wxT("imitone.com")))
	{
		wxMessageBox(_("Failed to reach the website for the error report.  Are you connected to the internet?"));
		post.Close();
		return;
	}
	
	wxInputStream *httpStream = post.GetInputStream(_T("/mothership/error_test.php"));
	
	if (post.GetError() == wxPROTO_NOERR)
	{
		wxString reply;
		wxStringOutputStream out_stream(&reply);
		httpStream->Read(out_stream);
	 
		wxMessageBox(wxT("WEBSITE REPLY:\r\n") + reply);
		// wxLogVerbose( wxString(_T(" returned document length: ")) << res.Length() );
	}
	else
	{
		wxMessageBox(wxString::Format(wxT("Failed to upload the report: error %i\r\n%s"),post.GetError()));
	}
	 
	wxDELETE(httpStream);
	post.Close();
	
	Close(true);
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
		wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE),
	report(_report)
{
	// Error display ?
	//wxTextCtrl *displayError 

	wxBoxSizer *sizerTop = new wxBoxSizer(wxVERTICAL);
	
	sizerTop->AddSpacer(MARGIN);

	if (report.promptMessage.length())
	{
		wxStaticText *messageText = new wxStaticText(this, -1, report.promptMessage,
			wxDefaultPosition, wxDefaultSize, wxCENTER);
		
		ApplyMarkup(messageText, report.promptMessage);
		
		sizerTop->Add(messageText, 0, wxALIGN_LEFT | wxALL, MARGIN);

		// Horizontal rule
		sizerTop->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, MARGIN);
	}

	wxFlexGridSizer *sizerField = NULL;

	for (Report::Parameters::iterator i = report.params.begin(); i != report.params.end(); ++i)
		switch (i->type)
	{
	case PARAM_FIELD:
		{
			wxStaticText *label = new wxStaticText(this, -1, i->label, wxDefaultPosition, wxDefaultSize, wxLEFT);
			
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
		}
		break;

	case PARAM_FIELD_MULTI:
		{
			if (sizerField) sizerField = NULL;

			wxStaticText *label = new wxStaticText(this, -1, i->label, wxDefaultPosition, wxDefaultSize, wxLEFT);

			wxTextCtrl *field = new wxTextCtrl(this, -1, i->value,
				wxDefaultPosition, wxSize(400, 200), wxTE_MULTILINE, wxDefaultValidator, i->name);
				
			Field fieldEntry = {&*i, field}; fields.push_back(fieldEntry);

			sizerTop->Add(label, 0, wxEXPAND | wxALL, MARGIN);
			sizerTop->Add(field, 0, wxEXPAND | wxALL, MARGIN);
		}
		
	default:
		// Not fields
		break;
	}

	{
		wxBoxSizer *actionRow = new wxBoxSizer(wxHORIZONTAL);

		wxButton
			*butSubmit = new wxButton(this, wxID_OK, _("Send Report")),
			*butCancel = new wxButton(this, wxID_CANCEL, _("Don't Send"));
			//*butDetails = new wxButton(this, wxID_VIEW_DETAILS, _("View Details..."));

		actionRow->Add(butSubmit, 1, wxEXPAND | wxALL, MARGIN);
		actionRow->Add(butCancel, 0, wxALL, MARGIN);
		//actionRow->Add(butDetails, 0, wxALL, MARGIN);

		butSubmit->SetDefault();

		sizerTop->Add(actionRow, 0, wxALIGN_CENTER | wxALL);
	}
	
	sizerTop->AddSpacer(MARGIN);

	// Apply the sizing scheme
	SetSizerAndFit(sizerTop);
}

void Prompt::OnClose(wxCloseEvent &event)
{
	// TODO consider veto
	Destroy();
}

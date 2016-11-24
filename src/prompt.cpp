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

static wxString GrabReplyTag(const wxString &reply, const wxString &tagName)
{
	wxString
		tagOpen  = wxT("<") +tagName+wxT(">"),
		tagClose = wxT("</")+tagName+wxT(">");
	
	int
		ptOpen = reply.Find(tagOpen),
		ptClose = reply.Find(tagClose);
	
	if (ptOpen != wxNOT_FOUND && ptClose != wxNOT_FOUND)
	{
		ptOpen += tagOpen.Length();
		if (ptOpen <= ptClose)
			return reply.Mid(ptOpen, ptClose-ptOpen);
	}
	
	return wxString();
}

void Prompt::OnSubmit(wxCommandEvent & event)
{
	UpdateReportFromFields();

	wxHTTP post;
	report.encodePost(post);
	
	post.SetTimeout(10);
	
	//wxString
	
	if (!post.Connect(report.uploadURL.base))
	{
		wxMessageBox(_("Failed to reach the website for the report.\nAre you connected to the internet?"));
		post.Close();
		return;
	}
	
	wxInputStream *httpStream = post.GetInputStream(report.uploadURL.path);
	
	if (post.GetError() == wxPROTO_NOERR)
	{
		wxString replyRaw;
		wxStringOutputStream out_stream(&replyRaw);
		httpStream->Read(out_stream);
		
		wxString
			replyTitle = GrabReplyTag(replyRaw, wxT("tattle-title")),
			replyMsg   = GrabReplyTag(replyRaw, wxT("tattle-message")),
			replyLink  = GrabReplyTag(replyRaw, wxT("tattle-link"));
		
		
		if (replyLink.Length() > 0)
		{
			replyLink = wxT("http://") + report.uploadURL.base + "/" + replyLink;

			if (!replyTitle.Length()) replyTitle = wxT("Suggested Link");
			if (!replyMsg  .Length()) replyMsg = wxT("The server suggests a solution.");
			
			replyMsg += wxT("\n\n") + replyLink +
				wxT("\nOpen this link in your browser?");
			
			int result = wxMessageBox(replyMsg, replyTitle, wxYES_NO|wxCENTER);
			
			if (result == wxYES)
			{
				wxLaunchDefaultBrowser(replyLink);
			}
		}
		else if (replyMsg.Length() > 0)
		{
			if (!replyTitle.Length()) replyTitle = "Report Sent!";
			
			wxMessageBox(replyMsg, replyTitle);
		}
		else
		{
			if (!replyTitle.Length()) replyTitle = "Report Sent";
			
			wxMessageBox(replyMsg, wxT("WEBSITE RESPONSE:\n\n") + replyRaw);
		}
	}
	else
	{
		int err_no = post.GetError();
		wxMessageBox(wxString::Format(wxT("Failed to upload the report: error %i"),err_no));
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
			*butSubmit = new wxButton(this, wxID_OK, _(report.labelSend)),
			*butCancel = new wxButton(this, wxID_CANCEL, _(report.labelCancel));
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

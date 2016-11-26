//
//  view_report.cpp
//  tattle
//
//  Created by Evan Balster on 11/25/16.
//

#include "tattle.h"

//Dialog stuff
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/statline.h>
#include <wx/sizer.h>

#if wxMAJOR_VERSION >= 3
	#include <wx/wrapsizer.h>
#endif

using namespace tattle;


wxBEGIN_EVENT_TABLE(ViewReport, wxDialog)
	EVT_BUTTON(ViewReport::Ev_Done,  ViewReport::OnDone)
	EVT_BUTTON(ViewReport::Ev_Open,  ViewReport::OnOpen)
	EVT_CLOSE (ViewReport::OnClose)
wxEND_EVENT_TABLE()

static const unsigned MARGIN = 5;


static int ViewReportCount = 0;

bool ViewReport::Exists()
{
	return ViewReportCount > 0;
}

ViewReport::~ViewReport()
{
	--ViewReportCount;
}


ViewReport::ViewReport(wxWindow * parent, wxWindowID id, Report &_report)
	: wxDialog(parent, id, wxT("Report Contents"),
		wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE),
	report(_report)
{
	++ViewReportCount;
	
	bool preQueryNote = false;
	
	wxBoxSizer *sizerTop = new wxBoxSizer(wxVERTICAL);
	
	sizerTop->AddSpacer(MARGIN);
	
	// Top label
	{
		wxStaticText *messageText = new wxStaticText(this, -1,
			wxT("This is all the information sent with your report."));
		
		sizerTop->Add(messageText, 0, wxALIGN_LEFT | wxALL, MARGIN);
	}
	
	// Horizontal rule
	sizerTop->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, MARGIN);
	
	// List string arguments
	{
		wxString argDump;
		
		for (Report::Parameters::iterator i = report.params.begin(); i != report.params.end(); ++i)
		{
			if (i->type != PARAM_STRING) continue;
			
			if (i->preQuery)
			{
				// Tell user which items are included in pre-query
				argDump += "* ";
				preQueryNote = true;
			}
			else argDump += "  ";
			
			if (i->value.find_first_of(wxT("\r\n")) != wxString::npos)
			{
				argDump += i->name + wxT(":\n")
					+ i->value + wxT("\n");
			}
			else
			{
				argDump += i->name + wxT(": ")
					+ i->value + wxT("\n");
			}
		}
		
		wxTextCtrl *argDisplay = new wxTextCtrl(this, -1, argDump,
			wxDefaultPosition, wxSize(360, 200),
			wxTE_MULTILINE|wxTE_READONLY, wxDefaultValidator);
		
		sizerTop->Add(argDisplay, 0, wxALIGN_LEFT | wxALL, MARGIN);
	}
	
	// List attached files
	{
	#if wxMAJOR_VERSION >= 3
		wxWrapSizer *sizerFiles = NULL;
	#else
		wxFlexGridSizer *sizerFiles = NULL;
	#endif
		
		for (Report::Parameters::iterator i = report.params.begin(); i != report.params.end(); ++i)
		{
			if (i->type != PARAM_FILE_TEXT && i->type != PARAM_FILE_BIN) continue;
			
			wxString shortName = i->fname;
			{
				size_t p = shortName.find_last_of("\\/");
				if (p != wxString::npos) shortName = shortName.substr(p+1);
			}
			
			if (!sizerFiles)
			{
			#if wxMAJOR_VERSION >= 3
				sizerFiles = new wxWrapSizer();
			#else
				sizerFiles = new wxFlexGridSizer(4);
			#endif
			
				sizerTop->Add(sizerFiles, 0, wxALL | wxALIGN_CENTER, 0);
			}

			wxButton *button = new wxButton(this, wxID_OPEN, wxT("File: ") + shortName,
				wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, i->name);
			
			sizerFiles->Add(button, 0, wxALL | wxALIGN_CENTER, MARGIN);
		}
	}
	
	// Horizontal rule
	sizerTop->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, MARGIN);
	
	if (preQueryNote && report.queryURL.isSet())
	{
		wxStaticText *asterisk = new wxStaticText(this, -1,
			wxT("* These were already used to search for a solution."));
		
		sizerTop->Add(asterisk, 0, wxALIGN_LEFT | wxALL, MARGIN);
	}
	
	{
		wxButton *butDone = new wxButton(this, wxID_OK);
		
		sizerTop->Add(butDone, 0, wxALIGN_CENTER | wxALL, MARGIN);
	}
	
	sizerTop->AddSpacer(MARGIN);

	SetSizerAndFit(sizerTop);
}

void ViewReport::OnDone (wxCommandEvent & event)
{
	EndModal(wxID_OK);
	Destroy();
}

void ViewReport::OnClose(wxCloseEvent   & event)
{
	EndModal(wxID_EXIT);
	Destroy();
}

void ViewReport::OnOpen (wxCommandEvent & event)
{
	wxWindow *window = dynamic_cast<wxWindow*>(event.GetEventObject());
	
	if (window)
	{
		Report::Parameter *param = report.findParam(window->GetName());
		
		if (param) wxLaunchDefaultApplication(param->fname);
	}
}


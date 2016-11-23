//
//  tattle.h
//  tattle
//
//  Created by Evan Balster on 11/12/16.
//  Copyright © 2016 imitone. All rights reserved.
//

#ifndef tattle_h
#define tattle_h

#include <string>
#include <iostream>
#include <list>

#if FORCE_TR1_TYPE_TRAITS
    // Hack to deal with STL weirdness on OS X
	#include <wx/setup.h>
    #include <wx/defs.h>
	#ifdef HAVE_TYPE_TRAITS
		#undef HAVE_TYPE_TRAITS
	#endif
	#ifndef HAVE_TR1_TYPE_TRAITS
		#define HAVE_TR1_TYPE_TRAITS
	#endif
#endif

#include <wx/string.h>
#include <wx/file.h>
#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/msgdlg.h>

#include <wx/protocol/http.h>

namespace tattle
{
	using std::cout;
	using std::endl;
	
	enum PARAM_TYPE
	{
		PARAM_NONE = 0,
		PARAM_STRING,
		PARAM_FILE_BIN,
		PARAM_FILE_TEXT,
		PARAM_FIELD,
		PARAM_FIELD_MULTI,
	};

	enum DETAIL_TYPE
	{
		DETAIL_NONE = 0,
		DETAIL_STRING = 1,
		DETAIL_TEXTFILE = 1,
		DETAIL_FILE = 2,
		DETAIL_DIR = 3,
	};
	
    /*
		Data structure defining the report.
	*/
    struct Report
    {
    public: //types
        
        struct Parameter
        {
            Parameter() : type(PARAM_NONE), trimBegin(0), trimEnd(0) {}
            
            PARAM_TYPE  type;
            wxString    name;
			
            // Filename if applicable
            wxString    fname;
            
            // String value or file content
            wxString    value;
			
			// Fields for user prompts only
            wxString    label, hint;
			
			// File truncation parameters
			unsigned    trimBegin, trimEnd;
			wxString    trimMark;
            
            // MIME content type and encoding information, if applicable.
            //   If non-empty, will be added to multipart POST request.
            std::string contentInfo;
        };
        
        typedef std::list<Parameter> Parameters;
        
        struct Detail
        {
            DETAIL_TYPE type;
            wxString    label;
            wxString    value;
        };
        
        typedef std::list<Detail> Details;
        
        Parameter *findParam(const wxString &name)
        {
            for (Parameters::iterator i = params.begin(); i != params.end(); ++i) if (i->name == name) return &*i;
            return NULL;
        }
        
        // Encode HTTP post request
        void encodePost(wxHTTP &request) const;
        
    public: // members
		
		struct UploadURL
		{
			wxString base, path;
			
			bool set(wxString fullURL);
		}
			uploadURL;
        
        wxString promptTitle;
        wxString promptMessage;
        
        bool silent;
        
        Parameters params;
        Details    details;
        
    public:
        
        Report() { silent = false; }
    };
    
    /*
		A prompt window which allows the user to enter data and submit the report.
    */
    class Prompt : public wxDialog
    {
    public:
        enum EventTypes
        {
            Ev_Exit    = wxID_EXIT,
            Ev_Cancel  = wxID_CANCEL,
            Ev_Submit  = wxID_OK,
            Ev_Details = wxID_VIEW_DETAILS,
        };
        
        Prompt(wxWindow * parent, wxWindowID id, Report &report);
        
    private:
        struct Field
        {
            Report::Parameter *param;
            wxTextCtrl        *control;
        };
        typedef std::vector<Field> Fields;
        
        void UpdateReportFromFields();
        
        void OnSubmit (wxCommandEvent & event);
        void OnCancel (wxCommandEvent & event);
        void OnDetails(wxCommandEvent & event);
        void OnClose  (wxCloseEvent   & event);
        
        Report &report;
        Fields  fields;
        
        wxDECLARE_EVENT_TABLE();
    };
	
	class TattleApp;
}

#endif /* tattle_h */

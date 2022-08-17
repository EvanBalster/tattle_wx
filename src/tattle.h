//
//  tattle.h
//  tattle
//
//  Created by Evan Balster on 11/12/16.
//

#ifndef tattle_h
#define tattle_h

#include <wx/defs.h>

#include <string>
#include <iostream>
#include <list>

#include <nlohmann/json.hpp>

#if FORCE_TR1_TYPE_TRAITS
    // Hack to deal with STL weirdness on OS X
	#include <wx/setup.h>
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
#include <wx/checkbox.h>
#include <wx/msgdlg.h>
#include <wx/artprov.h>
#include <wx/hyperlink.h>
#include <wx/progdlg.h>
#include <wx/event.h>


#include <wx/webrequest.h>

namespace tattle
{
	using std::cout;
	using std::endl;

	using Json = nlohmann::json;

	using JsonPointer = nlohmann::json::json_pointer;


	/*
	*	Failsafe JSON getter methods.
	*/
	template<typename T>
	T JsonMember(const Json &json, const Json::object_t::key_type &key, T &&defaultValue)
	{
		try {return json.value(key, std::forward<T>(defaultValue));}
		catch (Json::type_error &) {return defaultValue;}
	}
	inline std::string JsonMember(const Json &json, const Json::object_t::key_type &key, const char *default)
		{return JsonMember<std::string>(json, key, default);}

	template<typename T>
	T JsonFetch(const Json& json, const Json::json_pointer& ptr, const T &defaultValue)
		{try {return json.value(ptr, defaultValue);} catch (Json::type_error &) {return defaultValue;}}
	template<typename T>
	T JsonFetch(const Json& json, const char *ptr, const T &defaultValue)
		{return JsonFetch(json, Json::json_pointer(ptr), defaultValue);}
	
	inline std::string JsonFetch(const Json& json, const char *ptr, const char *defaultValue)
		{return JsonFetch<std::string>(json, Json::json_pointer(ptr), defaultValue);}


	/*
		Workflow control calls
	*/
	void Tattle_Proceed();
	void Tattle_ShowPrompt();
	void Tattle_InsertDialog(wxWindow *dialog);
	void Tattle_DisposeDialog(wxWindow *dialog);
	void Tattle_Halt();
	
	// Types of report parameters.
	enum PARAM_TYPE
	{
		PARAM_NONE = 0,
		PARAM_STRING,      // String provided in configuration
		PARAM_FILE_BIN,    // Data file referenced by configuration
		PARAM_FILE_TEXT,   // Text file referenced by configuration
		PARAM_FIELD,       // Text field
		PARAM_FIELD_MULTI, // Multi-line text field.
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
        /*
			Represents a report parameter.
		*/
        struct Parameter
        {
            Parameter();
            
            PARAM_TYPE  type;
            std::string name;
			bool        preQuery;  // Include in pre-query ?

			Json        json;

			std::string path()  const    {return JsonMember(json, "path", "");}

			std::string value() const    {if (json.is_string()) return json; else return JsonMember(json, "value", "");}

			bool        persist    () const    {return JsonMember(json, "persist", false);}

			std::string label      () const    {return JsonMember(json, "label", "");}
			std::string placeholder() const    {return JsonMember(json, "placeholder", "");}

			std::string content_info() const    {return JsonMember(json, "content_info", "");}

			unsigned    truncate_begin() const    {return JsonFetch(json, "/truncate/0", 0u);}
			unsigned    truncate_end  () const    {return JsonFetch(json, "/truncate/1", 0u);}
			std::string truncate_note () const    {return JsonFetch(json, "/truncate/2", "(trimmed)");}

			std::string input_warning() const    {return JsonMember(json, "input_warning", "");}
			
			// File contents
			wxMemoryBuffer fileContents;
        };
        
        using Parameters = std::list<Parameter>;
		
		/*
			Represents a parsed HTTP URL.
				base is an IP or hostname string, EG. uploads.example.com
				path is the path following the hostname, EG. /error/report.php
		*/
		struct ParsedURL
		{
			wxString scheme, host, path;
			unsigned long port;
			
			
			ParsedURL() {port=80;}
			
			// Set to a URL.  Only host, path and port will be used.
			bool set(wxString fullURL);
			
			bool isSet() const    {return host.Length() != 0;}


			wxString full() const
			{
				return scheme + "://" + host + ":" + wxString::Format(wxT("%d"),int(port)) + "/" + path;
			}
		};
		
		/*
			When performing a pre-query, the
		*/
		enum SERVER_COMMAND
		{
			SC_NONE         = 0, // No advice; proceed as usual
			SC_STOP         = 1, // Stop after displaying response
			SC_PROMPT       = 2, // Prompt after displaying response
			SC_STOP_ON_LINK = 3, // Halt only if link is followed
		};
		
		/*
			Represents the result of an HTTP query
		*/
		struct Reply
		{
			Reply();
			
			// HTTP status
			int                 statusCode = 0; // HTTP status, or 0 if server's response is not available.
			wxWebRequest::State requestState; // Web request state

			bool connected() const    {return statusCode != 0;} // TODO might be misleading if canceled partway
			
			wxString       raw;
			wxString       title, message, link;
			wxString       jsonValues;
			SERVER_COMMAND command;
			wxArtID        icon;
			
			bool ok()       const;
			bool valid()    const;
			bool sentLink() const;
			
			
			
			
			// Fill in string fields from HTTP request / stream
			void processResponse(wxWebRequest::State requestState, wxWebResponse &response, const ParsedURL &url, wxString query = wxT(""));
			
			// Assigns title, message and link based on raw reply text
			void parseRaw(const ParsedURL &url);
		};
		
		
	public:
		Report();
	
		Parameter       *findParam(const wxString &name);
		const Parameter *findParam(const wxString &name) const;
		
		// Read file contents into fileContents buffers
		void readFiles();
		
		
		// Query the server using the query address.

		/*
			Perform an HTTP query or HTTP post, or test the HTTP connection...
				Supply a parent window if possible, or some other event handler otherwise.
		*/
		Reply httpQuery(wxEvtHandler &parent) const;
		Reply httpPost (wxEvtHandler &parent) const;
		
		// Test connectivity by making a test connection (but no actual HTTP query)
		bool  httpTest(wxEvtHandler &parent, const ParsedURL &url) const;
		
        
        // Encode HTTP query and post request
		wxString preQueryString() const;
        void     encodePost(wxMemoryBuffer &postData, wxString boundary_id, bool preQuery) const;
        
    public: // members
		void httpAction(wxEvtHandler &handler, const ParsedURL &url, Reply &reply, wxProgressDialog *dlg, bool isQuery) const;

		Json config;

		Parameters params;

		std::string report_type() const    {return JsonFetch(config, "/data/$type", "");}
		std::string report_id  () const    {return JsonFetch(config, "/data/$id", "");}

		const ParsedURL& url_post()  const;
		const ParsedURL& url_query() const;

		bool enable_server_values() const    {return JsonFetch(config, "/config/server_values", true);}

		std::string path_reviewData() const    {return JsonFetch(config, "/paths/review", "");}
		std::string path_tattleData() const    {return JsonFetch(config, "/paths/state", "");}
		std::string path_tattleLog()  const    {return JsonFetch(config, "/paths/log", "");}
		
		bool connectionWarning;

	private:
		mutable struct
		{
			bool parsed = false;
			ParsedURL post, query;
		}
			url_cache;

		void _parse_urls() const;
    };

	struct UIConfig
	{
		UIConfig(Json &config);

		Json& config;

		std::string promptTitle    () const    {return JsonFetch(config, "/gui/title", "Report");}
		std::string promptMessage  () const    {return JsonFetch(config, "/gui/message", "Use this form to send us a report.");}
		std::string promptTechnical() const    {return JsonFetch(config, "/gui/technical", "");}
		std::string labelSend      () const    {return JsonFetch(config, "/gui/label_send", "");}
		std::string labelCancel    () const    {return JsonFetch(config, "/gui/label_cancel", "");}
		std::string labelReview    () const    {return JsonFetch(config, "/gui/label_review", "");}

		bool stayOnTop   () const    {return JsonFetch(config, "/gui/stay_on_top", false);}
		bool enableReview() const    {return JsonFetch(config, "/gui/review", false);}
		bool showProgress() const    {return JsonFetch(config, "/gui/progress_bar", false);}
		bool silentQuery () const    {return JsonFetch(config, "/gui/query", "default") == "silent";}
		bool silentPost  () const    {return JsonFetch(config, "/gui/post",  "default") == "silent";}

		// Margin sizes.
		unsigned marginSm() const;
		unsigned marginMd() const;
		unsigned marginLg() const;

		wxArtID defaultIcon() const    {return GetIconID(JsonFetch(config, "/gui/icon", ""));}

		wxArtID GetIconID(wxString tattleName) const;


		int style() const
		{
			return (stayOnTop() ? wxSTAY_ON_TOP : 0);
		}
	};

	/*
	*	Storage file for user input, user consent and server cookies.
	*/
	struct PersistentData
	{
	public:
		const Json &data;

		PersistentData();

		bool load      (wxString path);
		bool mergePatch(const Json &patch);

	private:
		wxString _path;
		Json     _data;
	};

	/*
		Globally shared report state and UI configuration.
			Only one report is made per invocation of Tattle.
	*/
	extern       PersistentData &persist;
	extern const Report         &report;
	extern const UIConfig       &uiConfig;
    
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
            Ev_Submit  = wxID_FORWARD,
            Ev_Details = wxID_INFO,
        };
        
        Prompt(wxWindow * parent, wxWindowID id, Report &report);
		~Prompt() wxOVERRIDE;
		
		/*
			Create a dialog box describing a server's reply.
				Returns true if the user followed a link.
		*/
		static wxWindow *DisplayReply(const Report::Reply &reply, wxWindow *parent = NULL);
        
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

		void OnShow   (wxShowEvent & event);
        
        Report &report;
        Fields  fields;
		
		wxFont fontTechnical;

		wxCheckBox *dontShowAgainBox = nullptr;
        
        wxDECLARE_EVENT_TABLE();
    };
	
	/*
		This dialog allows a user to view a report's contents
	*/
	class ViewReport : public wxDialog
    {
    public:
        enum EventTypes
        {
            Ev_Exit     = wxID_EXIT,
            Ev_Done     = wxID_OK,
			Ev_OpenFile = wxID_FILE,
			Ev_OpenDir  = wxID_OPEN,
        };
        
        ViewReport(wxWindow * parent, wxWindowID id);
		virtual ~ViewReport();
		
		static bool Exists();
        
    private:
        void OnDone    (wxCommandEvent & event);
		void OnOpenFile(wxCommandEvent & event);
		void OnOpenDir (wxCommandEvent & event);
        void OnClose   (wxCloseEvent   & event);
		
		wxFont fontTechnical;
        
        wxDECLARE_EVENT_TABLE();
    };

	/*
		Non-modal Dialogs integrated with Tattle's workflow.
			May be "OK" or "Open Link / Cancel" dialogs.
	*/
	class InfoDialog : public wxDialog
	{
	public:
		/*
			Create a Tattle dialog.
				- Specified title and message, with "OK" button
				- Optionally specify a link
				- Uses styling from uiConfig
				- Calls Tattle_Proceed / Tattle_Halt when done, depending on server command:
					- NONE:         proceed as usual
					- PROMPT:       return to prompt if applicable, else proceed
					- STOP:         always halt
					- STOP_ON_LINK: halt when following link, else proceed
		*/
		InfoDialog(wxWindow *parent,
			wxString title,
			wxString message,
			wxString link = wxString(),
			Report::SERVER_COMMAND command = Report::SC_NONE,
			wxArtID iconArtID = "");

	protected:
		void Done();

		void OpenLink();

		void OnOk(wxCommandEvent &evt);
		void OnLink(wxHyperlinkEvent &evt);
		void OnOpen(wxCommandEvent &evt);
		void OnCancel(wxCommandEvent &evt);

		void OnClose(wxCloseEvent &evt);

		wxDECLARE_EVENT_TABLE();

	protected:
		Report::SERVER_COMMAND command;

		wxString link;
		int      styleBase;

		bool     didAction;
		bool     overrideCommand;
	};
	
	// Utility functions
	wxString GetTagContents(const wxString &reply, const wxString &tagName);
	
	class TattleApp;
}

#endif /* tattle_h */

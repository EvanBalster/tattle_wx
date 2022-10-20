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


	enum
	{
		TATTLE_VERSION_MAJOR = 1,
		TATTLE_VERSION_MINOR = 0,
		TATTLE_VERSION_PATCH = 0,
	};


	/*
	*	Failsafe JSON getter methods.
	*/
	template<typename T>
	T JsonMember(const Json &json, const Json::object_t::key_type &key, T &&defaultValue)
	{
		try {return json.value(key, std::forward<T>(defaultValue));}
		catch (Json::type_error &) {return defaultValue;}
	}
	inline std::string JsonMember(const Json &json, const Json::object_t::key_type &key, const char *defaultValue)
		{return JsonMember<std::string>(json, key, defaultValue);}

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
		PARAM_STRING, // String provided in configuration
		PARAM_FILE,   // Text file referenced by configuration
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
		struct Identifier
		{
		public:
			std::string type;
			std::string id;

		public:
			explicit operator bool() const noexcept    {return type.length() || id.length();}

			Identifier() {}
			Identifier(std::string raw)
			{
				auto p = raw.find_first_of(':');
				if (p == std::string::npos) type = raw;
				else {type = raw.substr(0, p); id = raw.substr(p+1);}
			}
			Identifier(std::string _type, std::string _id)    : type(_type), id(_id) {}
		};

        /*
			Represents a report parameter.
		*/
        struct Content
        {
			Content();
            
            PARAM_TYPE  type;
            std::string name;
			bool        preQuery;  // Include in pre-query ?
			Json        json;      // Original JSON from configuration


			mutable std::string user_input;

			std::string value() const
			{
				if (user_input.length()) return user_input;
				if (json.is_string()) return json;
				return JsonMember(json, "value", "");
			}

			std::string path()  const    {return JsonMember(json, "path", "");}

			bool        persist    () const    {return JsonMember(json, "persist", false);}

			std::string label      () const    {return JsonMember(json, "label", "");}
			std::string placeholder() const    {return JsonMember(json, "placeholder", "");}

			std::string content_type             () const    {return JsonMember(json, "content-type", "application/octet-stream");}
			std::string content_transfer_encoding() const    {return JsonMember(json, "content-transfer-encoding", "");}

			unsigned    truncate_begin() const    {return JsonFetch(json, "/truncate/0", 0u);}
			unsigned    truncate_end  () const    {return JsonFetch(json, "/truncate/1", 0u);}
			std::string truncate_note () const    {return JsonFetch(json, "/truncate/2", "(trimmed)");}

			std::string input_warning() const    {return JsonMember(json, "input_warning", "");}
			
			// File contents
			wxMemoryBuffer fileContents;
        };
        
        using Contents = std::list<Content>;
		
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
			bool set(std::string fullURL);
			bool set(wxString    fullURL);
			
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
			Identifier     identity;
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

		/*
			Once the report is fully configured, call this function.
				- Generates parameters from JSON
				- Reads files into memory, trimmed
		*/
		void compile();
	
		/*
			Access report parameters.
		*/
		const Contents &contents()              const noexcept    {return _contents;}
		//Contents       &contents()                    noexcept    {return _contents;}
		Content        *findContent(const wxString &name);
		const Content  *findContent(const wxString &name) const;
		
		
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

		Identifier identity() const
		{
			return Identifier(
				JsonFetch(config, "/report/type", ""),
				JsonFetch(config, "/report/id",   ""));
		}

		const ParsedURL& url_post()  const;
		const ParsedURL& url_query() const;

		bool enable_server_values() const    {return JsonFetch(config, "/service/cookies", true);}

		std::string path_reviewData() const    {return JsonFetch(config, "/path/review", "");}
		std::string path_tattleData() const    {return JsonFetch(config, "/path/state", "");}
		std::string path_tattleLog()  const    {return JsonFetch(config, "/path/log", "");}
		
		bool connectionWarning;

	private:
		Contents _contents;

		mutable struct
		{
			bool parsed = false;
			ParsedURL post, query;
		}
			url_cache;

		void _parse_urls() const;
    };

	/*
	*	An interface to GUI configuration values.
	*/
	struct UIConfig
	{
		UIConfig(Json &config);

		Json& config;

		std::string promptTitle     () const    {return JsonFetch(config, "/text/prompt/title", "Report");}
		std::string promptMessage   () const    {return JsonFetch(config, "/text/prompt/message", "Use this form to send us a report.");}
		std::string promptTechnical () const    {return JsonFetch(config, "/report/summary", "");}
		std::string labelPrompt     () const    {return JsonFetch(config, "/text/prompt/hint", "");}
		std::string labelSend       () const    {return JsonFetch(config, "/text/prompt/btn_send", "");}
		std::string labelCancel     () const    {return JsonFetch(config, "/text/prompt/btn_cancel", "");}
		std::string labelReview     () const    {return JsonFetch(config, "/text/prompt/btn_review", "");}
		std::string labelHaltReports() const    {return JsonFetch(config, "/text/halt_reports", "Don't show again");}

		bool stayOnTop   () const    {return JsonFetch(config, "/gui/stay_on_top", false);}
		bool enableReview() const    {return JsonFetch(config, "/gui/prompt/review", false);}
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


		bool shouldShow(const Report::Identifier &id) const    {return JsonFetch(data, JsonPointer("/$show/"+id.type+"/"+id.id), 1) != 0;}


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
            const Report::Content *content;
            wxTextCtrl            *control;
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
			wxArtID iconArtID = "",
			Report::Identifier dontShowAgainID = {});

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

		Report::Identifier dontShowAgainID;
		wxCheckBox *dontShowAgainBox = nullptr;

		bool     didAction;
		bool     overrideCommand;
	};
	
	// Utility functions
	wxString GetTagContents(const wxString &reply, const wxString &tagName);
	
	class TattleApp;
}

#endif /* tattle_h */

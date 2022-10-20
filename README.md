# Tattle, a Consentful Tool for Phoning Home

Tattle is an interactive utility for applications that need to report to a server or fetch guidance for the user.  It is designed for error reporting, software update notifications, user feedback and data collection.  This process is interactive and designed to respect users' right to privacy and consent to data sharing.


## Workflow

Tattle is generally invoked by another application, which provides it with a configuration.  Tattle compiles a list of parameters, which can include text and file data as well as fields for the user to fill in.  Before asking the user for information, Tattle can make a GET request to the server in order to identify a problem and suggest a solution.

Step-by-step:

1. **Invocation**
  * Tattle is invoked with a [JSON command file](test/config.json) configuring the GUI, upload behaviors and attached data.
  * Attached files are truncated and read into memory immediately, in case they would subsequently change.
2. ❌ Planned: Consent to Query *(subject to privacy settings)*
   * ❌ Ask the user for consent to send basic information to the server.
   * This prompt is only shown to the user once per category.
3. **Query** _(if no query URL is supplied, this step is skipped)_
  * This is an HTTPS POST request, with a small subset of the report's data.
  * If the server responds with text or a link, this is displayed to the user.
  * If the server provides a `<tattle-id>` tag, the user gets a "don't show this again" option.
  * _The server may specify that execution should stop here.  (Not yet implemented!)_
  * This step happens only if the `-uq` flag is passed.
4. **Prompt** + Consent to Post _(`-s` bypasses)_
  * Display a prompt to the user, which may include an informative message and input fields.
  * The user may proceed using the **send** button, or abort using the **cancel** button.
  * The user may view the contents of the error report with the **view data** button.
5. **Post** _(if no post URL is supplied, this step is skipped)_
  * All parameters are encoded into an HTTP POST request and sent to the post URL.
  * If the post fails, the user is returned to the prompt.

Typically, Tattle displays a UI which will, at minimum, allow the user to either send the report or cancel it.  Any fields specified in the configuration will be displayed to the user, their contents submitted when the user chooses to send the report.


## Command Line

`tattle -h` displays command-line help.
`tattle <config.json>` invokes Tattle with the supplied command file.

[Additional command-line arguments](./Deprecated-Command-Line.md) are available to configure Tattle but have been deprecated in favor of the new command file format.

## About This Implementation

**tattle_wx** is based on the wxWidgets framework.  This enables portability to Windows, Mac OS X and Linux (and potentially others).  It adheres to the C++17 standard.

The author is interested in alternative implementations of this utility, in particular OS-native versions (which could minimize its footprint) and mobile versions.  A library implementation of Tattle may be useful for non-error reporting applications.


## Building Tattle/wx

Tattle requires a C++17 compiler and wxWidgets 3.  It depends on wxWidgets' **core**, **base** and **net** modules.

When development is complete I plan to include precompiled binaries here.


## Development Notes

Tattle is inspired by the Unity crash reporter, Mozilla crash reporter and Google Breakpad.  As far as I know it can be used as an uploader in a Breakpad-enabled app (but I haven't tried this yet)

Things I may investigate in the future:

* Allow Tattle to act as a child window of the parent application.
* Language files for localizing the UI
* GZIP-compressed uploading.
* Folder attachments.
* A way to save compiled reports to disk.
* Built-in / automatic parameters.
  * System description
  * Report time (local/GMT)

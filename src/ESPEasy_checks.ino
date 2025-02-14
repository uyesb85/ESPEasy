void run_compiletime_checks() {
  check_size<CRCStruct,                             168u>();
  check_size<SecurityStruct,                        593u>();
  const unsigned int SettingsStructSize = (244 + 82* TASKS_MAX);
  check_size<SettingsStruct,                        SettingsStructSize>();
  check_size<ControllerSettingsStruct,              748u>();
  check_size<NotificationSettingsStruct,            996u>();
  check_size<ExtraTaskSettingsStruct,               472u>();
  check_size<EventStruct,                           96u>();  // Is not stored

  // LogStruct is mainly dependent on the number of lines.
  // Has to be round up to multiple of 4.
  const unsigned int LogStructSize = ((12u + 17* LOG_STRUCT_MESSAGE_LINES) + 3) & ~3;
  check_size<LogStruct,                             LogStructSize >(); // Is not stored
  check_size<DeviceStruct,                          7u>();
  check_size<ProtocolStruct,                        10u>();
  check_size<NotificationStruct,                    3u>();
  check_size<NodeStruct,                            24u>();
  check_size<systemTimerStruct,                     28u>();
  check_size<RTCStruct,                             20u>();
  check_size<rulesTimerStatus,                      12u>();
  check_size<portStatusStruct,                      4u>();
  check_size<ResetFactoryDefaultPreference_struct,  4u>();
  check_size<GpioFactorySettingsStruct,             11u>();

}



String ReportOffsetErrorInStruct(const String& structname, size_t offset) {
  String error;
  error.reserve(48 + structname.length());
  error = F("Error: Incorrect offset in struct: ");
  error += structname;
  error += '(';
  error += String(offset);
  error += ')';
  return error;
}

/*********************************************************************************************\
 *  Analyze SettingsStruct and report inconsistencies
 *  Not a member function to be able to use the F-macro
\*********************************************************************************************/
bool SettingsCheck(String& error) {
  error = "";
#ifdef esp8266
  size_t offset = offsetof(SettingsStruct, ResetFactoryDefaultPreference);
  if (offset != 1224) {
    error = ReportOffsetErrorInStruct(F("SettingsStruct"), offset);
  }
#endif
  if (!Settings.networkSettingsEmpty()) {
    if (Settings.IP[0] == 0 || Settings.Gateway[0] == 0 || Settings.Subnet[0] == 0 || Settings.DNS[0] == 0) {
      error += F("Error: Either fill all IP settings fields or leave all empty");
    }
  }

  return error.length() == 0;
}

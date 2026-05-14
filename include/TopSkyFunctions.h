#pragma once

namespace TopSky
{
    // Label and Menu Operations
    constexpr static auto OPEN_EXTENDED_LABEL = 1;
    constexpr static auto EDIT_OP_TEXT2 = 2;
    constexpr static auto TOGGLE_UNITS = 3;
    constexpr static auto TOGGLE_CALLSIGN_HIGHLIGHT = 4;
    constexpr static auto TOGGLE_MARK = 5;
    constexpr static auto OPEN_CALLSIGN_MENU = 6;
    constexpr static auto TOGGLE_FREQ = 7;
    constexpr static auto TOGGLE_INBOUND_CLEARANCE_FLAG = 8;
    constexpr static auto TOGGLE_MILITARY_COORDINATION_FLAG = 9;
    constexpr static auto EDIT_OP_TEXT = 11;
    constexpr static auto OPEN_CFL_MENU = 12;
    constexpr static auto OPEN_AHDG_MENU = 14;
    constexpr static auto OPEN_ASP_MENU = 15;
    constexpr static auto OPEN_ARC_MENU = 16;
    constexpr static auto OPEN_AFL_MENU = 22;
    constexpr static auto TOGGLE_ATYP_HIGHLIGHT = 24;
    constexpr static auto OPEN_SQ_MENU = 27;
    constexpr static auto TOGGLE_MODE_S_DATA = 28;
    constexpr static auto OPEN_CFL_PEL_MENU_DEPRECATED = 44;
    constexpr static auto OPEN_WAYPOINT_MENU = 45;

    // Display Toggles (Label Fields)
    constexpr static auto TOGGLE_ADES_DISPLAY = 50;
    constexpr static auto TOGGLE_ATYP_W_DISPLAY = 51;
    constexpr static auto TOGGLE_GS_DISPLAY = 52;
    constexpr static auto TOGGLE_N_ATYP_DISPLAY = 53;
    constexpr static auto TOGGLE_WTC_DISPLAY = 54;
    constexpr static auto TOGGLE_ATYP_DISPLAY = 58;
    constexpr static auto OPEN_RFL_MENU = 59;
    constexpr static auto TOGGLE_FCOPX_DISPLAY = 61;
    constexpr static auto OPEN_ASSR_MENU = 62;
    constexpr static auto TOGGLE_CRC_DISPLAY = 63;
    constexpr static auto TOGGLE_RWY_DISPLAY = 64;
    constexpr static auto OPEN_STACK_MANAGER_WINDOW = 65;
    constexpr static auto TOGGLE_LABEL_DISPLAY = 67;
    constexpr static auto OPEN_PEL_MENU = 68;

    // Tactical and Coordination
    constexpr static auto OPEN_TACTICAL_TRANSFER_MENU = 69;
    constexpr static auto OPEN_TACTICAL_INFO_WINDOW = 70;
    constexpr static auto ACK_RJC_OR_FAIL_ASP_COORD_CLEAR_VALUE = 71;
    constexpr static auto TOGGLE_DSFL_DISPLAY = 72;
    constexpr static auto TOGGLE_DIAS_DISPLAY = 73;
    constexpr static auto TOGGLE_DMACH_DISPLAY = 74;
    constexpr static auto TOGGLE_DHDG_DISPLAY = 75;
    constexpr static auto TOGGLE_DRC_DISPLAY = 76;
    constexpr static auto TOGGLE_DGS_DISPLAY = 77;
    constexpr static auto OPEN_PDC_WINDOW = 78;

    // CPDLC and Data Link
    constexpr static auto OPEN_CPDLC_EMERGENCY_ACKNOWLEDGEMENT_MENU = 79;
    constexpr static auto CPDLC_WARNING_FUNCTIONS = 80;
    constexpr static auto SEND_CPDLC_SQUAWK_SSR = 81;
    constexpr static auto OPEN_CPDLC_CURRENT_MESSAGE_WINDOW_AHDG = 82;
    constexpr static auto OPEN_CPDLC_CURRENT_MESSAGE_WINDOW_ASP = 83;
    constexpr static auto OPEN_CPDLC_CURRENT_MESSAGE_WINDOW_RFL = 84;
    constexpr static auto OPEN_CPDLC_CURRENT_MESSAGE_WINDOW_NPT = 85;
    constexpr static auto ACKNOWLEDGE_PFREQ_ALERT = 86;
    constexpr static auto OPEN_CPDLC_CURRENT_MESSAGE_WINDOW_CPDLC_W = 88;

    // Sequencing and Time Management
    constexpr static auto OPEN_DSQ_MENU = 89;
    constexpr static auto DSQ_NUMBER_DECREMENT = 90;
    constexpr static auto DSQ_NUMBER_INCREMENT = 91;
    constexpr static auto OPEN_TIME_MENU_ETD = 92;
    constexpr static auto OPEN_TIME_MENU_ATD = 93;
    constexpr static auto OPEN_TIME_MENU_SLOT = 94;
    constexpr static auto OPEN_TIME_MENU_EOBT = 95;
    constexpr static auto OPEN_AHDG_MENU_DEP_LIST = 96;
    constexpr static auto NO_FUNCTION = 97;
    constexpr static auto RESET_ATD_TO_ETD = 98;
    constexpr static auto TOGGLE_ROUTE_DRAW_MTCD_SAP = 99;
    constexpr static auto ASSUME_TRANSFER = 100; // Duplicate ID in source table for FPL Window
    constexpr static auto OPEN_FPL_WINDOW = 100;
    constexpr static auto TOGGLE_EST_DEP_ABT = 101;
    constexpr static auto TOGGLE_GS_HIGHLIGHT = 103;
    constexpr static auto TOGGLE_WTC_HIGHLIGHT = 104;
    constexpr static auto OPEN_XFL_MENU = 105;
    constexpr static auto TOGGLE_WTG_HIGHLIGHT = 106;
    constexpr static auto TOGGLE_ENHANCED_AFL_DISPLAY = 107;
    constexpr static auto OPEN_AFL_MENU_TOGGLE_ENHANCED_AFL_DISPLAY = 108;
    constexpr static auto OPEN_CFL_MENU_DEP_LIST = 109;
    constexpr static auto OPEN_VERTICAL_AID_WINDOW = 110;
    constexpr static auto EDIT_FTEXT = 112;
    constexpr static auto EDIT_ATIS_LETTER = 114;
    constexpr static auto TOGGLE_ADES_HIGHLIGHT = 116;
    constexpr static auto TOGGLE_FLTID_TSSR_HIGHLIGHT = 117;
    constexpr static auto TOGGLE_CHECK_INDICATOR_ETWR = 118;
    constexpr static auto TOGGLE_NO_FIX_FLAG = 119;
    constexpr static auto TOGGLE_TML1_HOLD_XHOLD = 120;
    constexpr static auto TOGGLE_TML2_HOLD_XHOLD = 121;
    constexpr static auto OPEN_DCL_WINDOW = 122;
    constexpr static auto ACKNOWLEDGE_SID_ALLOCATION = 123;
    constexpr static auto ACKNOWLEDGE_STAR_ALLOCATION = 124;
    constexpr static auto OPEN_CMT_POP_UP = 125;

    // Check Indicators and Separation
    constexpr static auto TOGGLE_CHECK_INDICATOR_LOAD = 126;
    constexpr static auto TOGGLE_CHECK_INDICATOR_SEL = 127;
    constexpr static auto TOGGLE_CHECK_INDICATOR_SIL = 128;
    constexpr static auto TOGGLE_CHECK_INDICATOR_VFR = 129;
    constexpr static auto TOGGLE_CHECK_INDICATOR_TML1 = 130;
    constexpr static auto TOGGLE_CHECK_INDICATOR_TML2 = 131;
    constexpr static auto TOGGLE_CHECK_INDICATOR_RESECT = 132;
    constexpr static auto ACK_RJC_OR_FAIL_AHDG_COORD_CLEAR_VALUE_ASSIGN_PRESENT_HEADING = 133;
    constexpr static auto ACK_RJC_OR_FAIL_ARC_COORD_CLEAR_VALUE = 134;
    constexpr static auto INVOKE_SEP_TOOL = 135;
    constexpr static auto INVOKE_SEP_TOOL_WITH_VSEP = 136;
    constexpr static auto FIND_ASSR = 137;
    constexpr static auto FIND_PSSR = 138;
    constexpr static auto OPEN_CFL_PEL_MENU = 139;

    // Route Drawing and OCM
    constexpr static auto TOGGLE_ROUTE_DRAW_MTCD_ONLY_WITH_AUTOHIDE = 140;
    constexpr static auto TOGGLE_ROUTE_DRAW_MTCD_ONLY = 141;
    constexpr static auto TOGGLE_ROUTE_DRAW_SAP_ONLY_WITH_AUTOHIDE = 142;
    constexpr static auto TOGGLE_ROUTE_DRAW_SAP_ONLY = 143;
    constexpr static auto ACKNOWLEDGE_OCM_ALERT = 144;
    constexpr static auto ACKNOWLEDGE_CLEAR_OCM_NBT = 145;
    constexpr static auto ACKNOWLEDGE_CLEAR_OCM_NLT = 146;
    constexpr static auto OPEN_TIME_MENU_NBT = 147;
    constexpr static auto OPEN_TIME_MENU_NLT = 148;
    constexpr static auto TOGGLE_OCEANIC_LEVEL_HIGHLIGHT = 149;
    constexpr static auto ACKNOWLEDGE_OCM_ALERT_TOGGLE_OFL_HIGHLIGHT = 150;
    constexpr static auto TOGGLE_LEVEL_BAND_HIGHLIGHT_AFL = 151;
    constexpr static auto TOGGLE_LEVEL_BAND_HIGHLIGHT_XFL = 152;
    constexpr static auto ACKNOWLEDGE_AIW_ALERT = 153;
    constexpr static auto OPEN_OCEANIC_TIME_RESTRICTION_WINDOW = 154;
    constexpr static auto TOGGLE_ACF_FIELD_COLOR = 156;
    constexpr static auto CLEAR_APPROACH_CLEARANCE = 157;
    constexpr static auto TOGGLE_MINIMISED_LABEL = 158;
    constexpr static auto OPEN_EXTENDED_LABEL_CLICK_TOGGLE_MINIMISED_LABEL_SHIFT_CLICK = 159;
    constexpr static auto TOGGLE_AFL_HIGHLIGHT = 160;
    constexpr static auto OPEN_AFL_MENU_TOGGLE_AFL_HIGHLIGHT = 161;
    constexpr static auto TOGGLE_TTLTTG_DISPLAY = 162;
    constexpr static auto TOGGLE_CHECK_INDICATOR_DEP = 163;
    constexpr static auto OPEN_TIME_MENU_EAT = 164;
    constexpr static auto TOGGLE_EAT_GIVEN_INDICATOR = 165;
    constexpr static auto TOGGLE_RFL_DISPLAY = 166;
    constexpr static auto FSQ_RE_SEQUENCE = 167;
    constexpr static auto FSQ_REMOVE = 168;
}
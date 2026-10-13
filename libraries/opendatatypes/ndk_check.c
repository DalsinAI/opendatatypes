/* OpenDatatypes: every number and layout dt_core.h carries, checked against
 * the NDK 3.2 headers at compile time. Built (never run) by build-amiga.sh
 * on the stove; a mismatch stops the build. Names an older NDK may not carry
 * (V44/V45 additions) are checked when the headers have them.
 * Copyright (c) 2026 Dalsin Limited. MIT.
 */
#include <stddef.h>
#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/lists.h>
#include <utility/tagitem.h>
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include "dt_core.h"

#define SAME(a, b) _Static_assert((unsigned long)(a) == (unsigned long)(b), #a " is not " #b)
#define AT(ours, field, theirs, tfield) SAME(offsetof(struct ours, field), offsetof(struct theirs, tfield))
#define SIZE(ours, theirs) SAME(sizeof(struct ours), sizeof(struct theirs))

SIZE(odt_Node, Node);
SIZE(odt_List, List);
SIZE(odt_TagItem, TagItem);
SAME(ODT_TAG_DONE, TAG_DONE); SAME(ODT_TAG_IGNORE, TAG_IGNORE);
SAME(ODT_TAG_MORE, TAG_MORE); SAME(ODT_TAG_SKIP, TAG_SKIP);

SAME(ODT_ID_DTYP, ID_DTYP); SAME(ODT_ID_DTHD, ID_DTHD); SAME(ODT_ID_CODE, ID_CODE);
SAME(ODT_ID_TOOL, ID_TOOL); SAME(ODT_ID_TAGS, ID_TAGS); SAME(ODT_ID_NAME, ID_NAME);

SIZE(odt_DataTypeHeader, DataTypeHeader);
SAME(sizeof(struct odt_DataTypeHeader), DTHSIZE);
AT(odt_DataTypeHeader, dth_Name, DataTypeHeader, dth_Name);
AT(odt_DataTypeHeader, dth_BaseName, DataTypeHeader, dth_BaseName);
AT(odt_DataTypeHeader, dth_Pattern, DataTypeHeader, dth_Pattern);
AT(odt_DataTypeHeader, dth_Mask, DataTypeHeader, dth_Mask);
AT(odt_DataTypeHeader, dth_GroupID, DataTypeHeader, dth_GroupID);
AT(odt_DataTypeHeader, dth_ID, DataTypeHeader, dth_ID);
AT(odt_DataTypeHeader, dth_MaskLen, DataTypeHeader, dth_MaskLen);
AT(odt_DataTypeHeader, dth_Pad, DataTypeHeader, dth_Pad);
AT(odt_DataTypeHeader, dth_Flags, DataTypeHeader, dth_Flags);
AT(odt_DataTypeHeader, dth_Priority, DataTypeHeader, dth_Priority);
SAME(ODT_DTF_TYPE_MASK, DTF_TYPE_MASK); SAME(ODT_DTF_BINARY, DTF_BINARY); SAME(ODT_DTF_ASCII, DTF_ASCII);
SAME(ODT_DTF_IFF, DTF_IFF); SAME(ODT_DTF_MISC, DTF_MISC); SAME(ODT_DTF_CASE, DTF_CASE);
SAME(ODT_DTF_SYSTEM1, DTF_SYSTEM1);
SAME(ODT_GID_SYSTEM, GID_SYSTEM); SAME(ODT_GID_TEXT, GID_TEXT); SAME(ODT_GID_DOCUMENT, GID_DOCUMENT);
SAME(ODT_GID_SOUND, GID_SOUND); SAME(ODT_GID_INSTRUMENT, GID_INSTRUMENT); SAME(ODT_GID_MUSIC, GID_MUSIC);
SAME(ODT_GID_PICTURE, GID_PICTURE); SAME(ODT_GID_ANIMATION, GID_ANIMATION); SAME(ODT_GID_MOVIE, GID_MOVIE);

SIZE(odt_DTHookContext, DTHookContext);
AT(odt_DTHookContext, dthc_Lock, DTHookContext, dthc_Lock);
AT(odt_DTHookContext, dthc_FIB, DTHookContext, dthc_FIB);
AT(odt_DTHookContext, dthc_FileHandle, DTHookContext, dthc_FileHandle);
AT(odt_DTHookContext, dthc_IFF, DTHookContext, dthc_IFF);
AT(odt_DTHookContext, dthc_Buffer, DTHookContext, dthc_Buffer);
AT(odt_DTHookContext, dthc_BufferLength, DTHookContext, dthc_BufferLength);

SIZE(odt_Tool, Tool);
AT(odt_Tool, tn_Which, Tool, tn_Which);
AT(odt_Tool, tn_Flags, Tool, tn_Flags);
AT(odt_Tool, tn_Program, Tool, tn_Program);
SAME(ODT_TW_INFO, TW_INFO); SAME(ODT_TW_BROWSE, TW_BROWSE); SAME(ODT_TW_EDIT, TW_EDIT);
SAME(ODT_TW_PRINT, TW_PRINT); SAME(ODT_TW_MAIL, TW_MAIL);
SAME(ODT_TF_LAUNCH_MASK, TF_LAUNCH_MASK); SAME(ODT_TF_SHELL, TF_SHELL);
SAME(ODT_TF_WORKBENCH, TF_WORKBENCH); SAME(ODT_TF_RX, TF_RX);

SIZE(odt_DataType, DataType);
AT(odt_DataType, dtn_Node1, DataType, dtn_Node1);
AT(odt_DataType, dtn_Node2, DataType, dtn_Node2);
AT(odt_DataType, dtn_Header, DataType, dtn_Header);
AT(odt_DataType, dtn_ToolList, DataType, dtn_ToolList);
AT(odt_DataType, dtn_FunctionName, DataType, dtn_FunctionName);
AT(odt_DataType, dtn_AttrList, DataType, dtn_AttrList);
AT(odt_DataType, dtn_Length, DataType, dtn_Length);

SIZE(odt_ToolNode, ToolNode);
AT(odt_ToolNode, tn_Tool, ToolNode, tn_Tool);
AT(odt_ToolNode, tn_Length, ToolNode, tn_Length);

SAME(ODT_DTST_RAM, DTST_RAM); SAME(ODT_DTST_FILE, DTST_FILE); SAME(ODT_DTST_CLIPBOARD, DTST_CLIPBOARD);
SAME(ODT_DTST_HOTLINK, DTST_HOTLINK);
#ifdef DTST_MEMORY
SAME(ODT_DTST_MEMORY, DTST_MEMORY);
#endif

SAME(ODT_DTERROR_UNKNOWN_DATATYPE, DTERROR_UNKNOWN_DATATYPE);
SAME(ODT_DTERROR_COULDNT_SAVE, DTERROR_COULDNT_SAVE);
SAME(ODT_DTERROR_COULDNT_OPEN, DTERROR_COULDNT_OPEN);
SAME(ODT_DTERROR_COULDNT_SEND_MESSAGE, DTERROR_COULDNT_SEND_MESSAGE);
SAME(ODT_DTERROR_COULDNT_OPEN_CLIPBOARD, DTERROR_COULDNT_OPEN_CLIPBOARD);
SAME(ODT_DTERROR_UNKNOWN_COMPRESSION, DTERROR_UNKNOWN_COMPRESSION);
SAME(ODT_DTERROR_NOT_ENOUGH_DATA, DTERROR_NOT_ENOUGH_DATA);
SAME(ODT_DTERROR_INVALID_DATA, DTERROR_INVALID_DATA);
#ifdef DTERROR_NOT_AVAILABLE
SAME(ODT_DTERROR_NOT_AVAILABLE, DTERROR_NOT_AVAILABLE);
#endif
SAME(ODT_DTMSG_TYPE_OFFSET, DTMSG_TYPE_OFFSET);

SIZE(odt_DTMethod, DTMethod);
AT(odt_DTMethod, dtm_Command, DTMethod, dtm_Command);
AT(odt_DTMethod, dtm_Method, DTMethod, dtm_Method);
SAME(ODT_STM_PAUSE, STM_PAUSE); SAME(ODT_STM_PLAY, STM_PLAY); SAME(ODT_STM_CONTENTS, STM_CONTENTS);
SAME(ODT_STM_INDEX, STM_INDEX); SAME(ODT_STM_RETRACE, STM_RETRACE);
SAME(ODT_STM_BROWSE_PREV, STM_BROWSE_PREV); SAME(ODT_STM_BROWSE_NEXT, STM_BROWSE_NEXT);
SAME(ODT_STM_NEXT_FIELD, STM_NEXT_FIELD); SAME(ODT_STM_PREV_FIELD, STM_PREV_FIELD);
SAME(ODT_STM_ACTIVATE_FIELD, STM_ACTIVATE_FIELD); SAME(ODT_STM_COMMAND, STM_COMMAND);
SAME(ODT_STM_REWIND, STM_REWIND); SAME(ODT_STM_FASTFORWARD, STM_FASTFORWARD);
SAME(ODT_STM_STOP, STM_STOP); SAME(ODT_STM_RESUME, STM_RESUME); SAME(ODT_STM_LOCATE, STM_LOCATE);
#ifdef STM_HELP
SAME(ODT_STM_HELP, STM_HELP);
#endif
#ifdef STM_SEARCH
SAME(ODT_STM_SEARCH, STM_SEARCH);
#endif
#ifdef STM_SEARCH_NEXT
SAME(ODT_STM_SEARCH_NEXT, STM_SEARCH_NEXT);
#endif
#ifdef STM_SEARCH_PREV
SAME(ODT_STM_SEARCH_PREV, STM_SEARCH_PREV);
#endif
#ifdef STM_USER
SAME(ODT_STM_USER, STM_USER);
#endif
#ifdef STMF_METHOD_MASK
SAME(ODT_STMF_METHOD_MASK, STMF_METHOD_MASK);
#endif
#ifdef STMF_DATA_MASK
SAME(ODT_STMF_DATA_MASK, STMF_DATA_MASK);
#endif
#ifdef STMF_RESERVED_MASK
SAME(ODT_STMF_RESERVED_MASK, STMF_RESERVED_MASK);
#endif
#ifdef STMD_VOID
SAME(ODT_STMD_VOID, STMD_VOID);
#endif
#ifdef STMD_ULONG
SAME(ODT_STMD_ULONG, STMD_ULONG);
#endif
#ifdef STMD_STRPTR
SAME(ODT_STMD_STRPTR, STMD_STRPTR);
#endif
#ifdef STMD_TAGLIST
SAME(ODT_STMD_TAGLIST, STMD_TAGLIST);
#endif

#ifdef TOOLA_Program
SAME(ODT_TOOLA_Program, TOOLA_Program);
#endif
#ifdef TOOLA_Which
SAME(ODT_TOOLA_Which, TOOLA_Which);
#endif
#ifdef TOOLA_LaunchType
SAME(ODT_TOOLA_LaunchType, TOOLA_LaunchType);
#endif

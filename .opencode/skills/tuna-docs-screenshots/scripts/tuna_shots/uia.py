"""A minimal UI Automation client implemented with ``ctypes`` (stdlib only).

Dolphin exposes its TAS input controls through UI Automation, so the toolkit can
toggle emulated buttons and open menus without ever sending a keystroke to the
focused window (which the emulator docs forbid).

Only the handful of interfaces actually used are declared. Vtable slots follow
``UIAutomationClient.h``; every offset below is asserted by a comment so the
ABI contract stays reviewable.
"""
from __future__ import annotations

import ctypes
import uuid
from ctypes import wintypes
from typing import List, Optional

ole32 = ctypes.WinDLL("ole32", use_last_error=True)
oleaut32 = ctypes.WinDLL("oleaut32", use_last_error=True)

HRESULT = ctypes.c_long
CLSCTX_INPROC_SERVER = 1
COINIT_MULTITHREADED = 0

# --- UIA identifiers ---------------------------------------------------------
CLSID_CUIAutomation = "FF48DBA4-60EF-4201-AA87-54103EEF594E"
IID_IUIAutomation = "30CBE57D-D9D0-452A-AB13-7AC5AC4825EE"
IID_IUIAutomationElement = "D22108AA-8AC5-49A5-837B-37BBB3D7591E"

UIA_NamePropertyId = 30005
UIA_AutomationIdPropertyId = 30011

UIA_InvokePatternId = 10000
UIA_ExpandCollapsePatternId = 10005
UIA_TogglePatternId = 10015

TreeScope_Descendants = 4

ToggleState_Off = 0
ToggleState_On = 1
ToggleState_Indeterminate = 2

VT_I4 = 3
VT_BSTR = 8


class GUID(ctypes.Structure):
    _fields_ = [
        ("Data1", wintypes.DWORD),
        ("Data2", wintypes.WORD),
        ("Data3", wintypes.WORD),
        ("Data4", ctypes.c_byte * 8),
    ]


def _guid(value: str) -> GUID:
    guid = GUID()
    ctypes.memmove(ctypes.byref(guid), uuid.UUID(value).bytes_le, 16)
    return guid


class _VARIANTUNION(ctypes.Union):
    _fields_ = [
        ("lVal", ctypes.c_long),
        ("llVal", ctypes.c_longlong),
        ("bstrVal", ctypes.c_void_p),
        ("punkVal", ctypes.c_void_p),
        ("_reserved", ctypes.c_byte * 16),
    ]


class VARIANT(ctypes.Structure):
    # x86 and x64 share a 24-byte VARIANT: 8 bytes of header plus a 16-byte union.
    _fields_ = [
        ("vt", ctypes.c_ushort),
        ("wReserved1", ctypes.c_ushort),
        ("wReserved2", ctypes.c_ushort),
        ("wReserved3", ctypes.c_ushort),
        ("value", _VARIANTUNION),
    ]


ole32.CoInitializeEx.argtypes = [ctypes.c_void_p, wintypes.DWORD]
ole32.CoInitializeEx.restype = HRESULT
ole32.CoUninitialize.argtypes = []
ole32.CoCreateInstance.argtypes = [
    ctypes.POINTER(GUID),
    ctypes.c_void_p,
    wintypes.DWORD,
    ctypes.POINTER(GUID),
    ctypes.POINTER(ctypes.c_void_p),
]
ole32.CoCreateInstance.restype = HRESULT
oleaut32.SysAllocString.argtypes = [wintypes.LPCWSTR]
oleaut32.SysAllocString.restype = ctypes.c_void_p
oleaut32.SysFreeString.argtypes = [ctypes.c_void_p]
oleaut32.VariantClear.argtypes = [ctypes.POINTER(VARIANT)]
oleaut32.VariantClear.restype = HRESULT


def _check(hr: int, what: str) -> None:
    if hr < 0:
        raise OSError(f"UI Automation call failed ({what}): HRESULT 0x{hr & 0xFFFFFFFF:08X}")


def _method(ptr: int, index: int, restype, *argtypes):
    """Build a callable for vtable slot ``index`` of the interface at ``ptr``."""
    vtable = ctypes.cast(
        ctypes.c_void_p(ptr), ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))
    )[0]
    proto = ctypes.WINFUNCTYPE(restype, ctypes.c_void_p, *argtypes)
    return proto(vtable[index])


class Automation:
    """Owns the IUIAutomation object and the COM apartment it lives in."""

    def __init__(self) -> None:
        _check(ole32.CoInitializeEx(None, COINIT_MULTITHREADED), "CoInitializeEx")
        self._ptr = ctypes.c_void_p()
        clsid = _guid(CLSID_CUIAutomation)
        iid = _guid(IID_IUIAutomation)
        _check(
            ole32.CoCreateInstance(
                ctypes.byref(clsid), None, CLSCTX_INPROC_SERVER, ctypes.byref(iid),
                ctypes.byref(self._ptr),
            ),
            "CoCreateInstance(CUIAutomation)",
        )
        self._ptr = self._ptr.value

    def close(self) -> None:
        if self._ptr:
            self._release(self._ptr)
            self._ptr = 0
            ole32.CoUninitialize()

    def __enter__(self) -> "Automation":
        return self

    def __exit__(self, *_exc) -> None:
        self.close()

    # --- lifecycle -----------------------------------------------------------
    def _release(self, ptr: int) -> None:
        if ptr:
            _method(ptr, 2, HRESULT)(ptr)  # IUnknown::Release

    def element_from_handle(self, hwnd: int) -> int:
        element = ctypes.c_void_p()
        # IUIAutomation::ElementFromHandle = slot 6
        _check(_method(self._ptr, 6, HRESULT, wintypes.HWND, ctypes.POINTER(ctypes.c_void_p))(
            self._ptr, hwnd, ctypes.byref(element)
        ), "ElementFromHandle")
        return element.value

    def _name_condition(self, name: str) -> int:
        variant = VARIANT()
        variant.vt = VT_BSTR
        bstr = oleaut32.SysAllocString(name)
        variant.value.bstrVal = bstr
        condition = ctypes.c_void_p()
        try:
            # IUIAutomation::CreatePropertyCondition = slot 23
            _check(_method(
                self._ptr, 23, HRESULT, ctypes.c_int, VARIANT, ctypes.POINTER(ctypes.c_void_p)
            )(self._ptr, UIA_NamePropertyId, variant, ctypes.byref(condition)), "CreatePropertyCondition")
        finally:
            oleaut32.SysFreeString(bstr)
        return condition.value

    def find_first(self, parent: int, name: str, scope: int = TreeScope_Descendants) -> Optional[int]:
        condition = self._name_condition(name)
        try:
            found = ctypes.c_void_p()
            # IUIAutomationElement::FindFirst = slot 5
            hr = _method(parent, 5, HRESULT, ctypes.c_int, ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p))(
                parent, scope, condition, ctypes.byref(found)
            )
            _check(hr, "FindFirst")
            return found.value or None
        finally:
            self._release(condition)

    def find_all(self, parent: int, name: str, scope: int = TreeScope_Descendants) -> List[int]:
        condition = self._name_condition(name)
        try:
            array = ctypes.c_void_p()
            # IUIAutomationElement::FindAll = slot 6
            hr = _method(parent, 6, HRESULT, ctypes.c_int, ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p))(
                parent, scope, condition, ctypes.byref(array)
            )
            _check(hr, "FindAll")
            if not array.value:
                return []
            length = ctypes.c_int()
            # IUIAutomationElementArray::get_Length = slot 3
            _check(_method(array.value, 3, HRESULT, ctypes.POINTER(ctypes.c_int))(
                array.value, ctypes.byref(length)), "ElementArray.Length")
            items: List[int] = []
            for i in range(length.value):
                element = ctypes.c_void_p()
                # IUIAutomationElementArray::GetElement = slot 4
                _check(_method(array.value, 4, HRESULT, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p))(
                    array.value, i, ctypes.byref(element)), "ElementArray.GetElement")
                if element.value:
                    items.append(element.value)
            self._release(array.value)
            return items
        finally:
            self._release(condition)

    def get_pattern(self, element: int, pattern_id: int) -> Optional[int]:
        pattern = ctypes.c_void_p()
        # IUIAutomationElement::GetCurrentPattern = slot 16
        _check(_method(element, 16, HRESULT, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p))(
            element, pattern_id, ctypes.byref(pattern)), "GetCurrentPattern")
        return pattern.value or None

    # --- pattern helpers -----------------------------------------------------
    def toggle_state(self, pattern: int) -> int:
        # IUIAutomationTogglePattern::get_CurrentToggleState = slot 4.
        # Pattern property getters return the raw enum, not a VARIANT.
        state = ctypes.c_int()
        _check(_method(pattern, 4, HRESULT, ctypes.POINTER(ctypes.c_int))(
            pattern, ctypes.byref(state)), "get_CurrentToggleState")
        return int(state.value)

    def set_toggle_state(self, pattern: int, state: int) -> None:
        # IUIAutomationTogglePattern::SetToggleState = slot 3
        _check(_method(pattern, 3, HRESULT, ctypes.c_int)(
            pattern, state), "SetToggleState")

    def invoke(self, pattern: int) -> None:
        # IUIAutomationInvokePattern::Invoke = slot 3
        _check(_method(pattern, 3, HRESULT)(pattern), "Invoke")

    def expand(self, pattern: int) -> None:
        # IUIAutomationExpandCollapsePattern::Expand = slot 3
        _check(_method(pattern, 3, HRESULT)(pattern), "Expand")

    def release(self, ptr: Optional[int]) -> None:
        self._release(ptr or 0)

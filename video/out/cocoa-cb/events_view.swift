/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

import Cocoa

class EventsView: NSView {

    weak var cocoaCB: CocoaCB!
    var mpv: MPVHelper! {
        get { return cocoaCB == nil ? nil : cocoaCB.mpv }
    }

    var tracker: NSTrackingArea?
    var hasMouseDown: Bool = false

    override var isFlipped: Bool { return true }
    override var acceptsFirstResponder: Bool { return true }

    static let fileURLType: NSPasteboard.PasteboardType = {
        if #available(OSX 10.13, *) {
            return NSPasteboard.PasteboardType.fileURL
        } else {
            return NSPasteboard.PasteboardType(kUTTypeFileURL as String)
        }
    } ()

    static let URLType: NSPasteboard.PasteboardType = {
        if #available(OSX 10.13, *) {
            return NSPasteboard.PasteboardType.URL
        } else {
            return NSPasteboard.PasteboardType(kUTTypeURL as String)
        }
    } ()

    init(cocoaCB ccb: CocoaCB) {
        cocoaCB = ccb
        super.init(frame: NSMakeRect(0, 0, 960, 480))
        autoresizingMask = [.width, .height]
        wantsBestResolutionOpenGLSurface = true
        registerForDraggedTypes([ EventsView.fileURLType,
                                  EventsView.URLType,
                                  NSPasteboard.PasteboardType.string ])
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func updateTrackingAreas() {
        if tracker != nil {
            removeTrackingArea(tracker!)
        }

        tracker = NSTrackingArea(rect: bounds,
            options: [.activeAlways, .mouseEnteredAndExited, .mouseMoved, .enabledDuringMouseDrag],
            owner: self, userInfo: nil)
        addTrackingArea(tracker!)

        if containsMouseLocation() {
            cocoa_put_key_with_modifiers(SWIFT_KEY_MOUSE_LEAVE, 0)
        }
    }

    override func draggingEntered(_ sender: NSDraggingInfo) -> NSDragOperation {
#if swift(>=5.0)
        guard let types = sender.draggingPasteboard.types else { return [] }
#else
        guard let types = sender.draggingPasteboard().types else { return [] }
#endif
        if types.contains(EventsView.fileURLType) ||
           types.contains(EventsView.URLType) ||
           types.contains(NSPasteboard.PasteboardType.string)
        {
            return .copy
        }
        return []
    }

    func isURL(_ str: String) -> Bool {
        let regex = try! NSRegularExpression(pattern: "^(https?|ftp)://[^\\s/$.?#].[^\\s]*$",
                                             options: .caseInsensitive)
        let isURL = regex.numberOfMatches(in: str,
                                     options: [],
                                       range: NSRange(location: 0, length: str.count))
        return isURL > 0
    }

    override func performDragOperation(_ sender: NSDraggingInfo) -> Bool {
#if swift(>=5.0)
        let pb = sender.draggingPasteboard
#else
        let pb = sender.draggingPasteboard()
#endif
        guard let types = pb.types else { return false }
        if types.contains(EventsView.fileURLType) {
            if let files = pb.propertyList(forType: EventsView.fileURLType) as? [Any] {
                EventsResponder.sharedInstance().handleFilesArray(files)
                return true
            }
        } else if types.contains(EventsView.URLType) {
            if var url = pb.propertyList(forType: EventsView.URLType) as? [String] {
                url = url.filter{ !$0.isEmpty }
                EventsResponder.sharedInstance().handleFilesArray(url)
                return true
            }
        } else if types.contains(NSPasteboard.PasteboardType.string) {
            guard let str = pb.string(forType: NSPasteboard.PasteboardType.string) else { return false }
            var filesArray: [String] = []

            for val in str.components(separatedBy: "\n") {
                let url = val.trimmingCharacters(in: .whitespacesAndNewlines)
                let path = (url as NSString).expandingTildeInPath
                if isURL(url) {
                    filesArray.append(url)
                } else if path.starts(with: "/") {
                    filesArray.append(path)
                }
            }
            EventsResponder.sharedInstance().handleFilesArray(filesArray)
            return true
        }
        return false
    }

    override func acceptsFirstMouse(for event: NSEvent?) -> Bool {
        return true
    }

    override func becomeFirstResponder() -> Bool {
        return true
    }

    override func resignFirstResponder() -> Bool {
        return true
    }

    override func mouseEntered(with event: NSEvent) {
        if mpv.getBoolProperty("input-cursor") {
            cocoa_put_key_with_modifiers(SWIFT_KEY_MOUSE_ENTER, 0)
        }
    }

    override func mouseExited(with event: NSEvent) {
        if mpv.getBoolProperty("input-cursor") {
            cocoa_put_key_with_modifiers(SWIFT_KEY_MOUSE_LEAVE, 0)
        }
        cocoaCB.titleBar.hide()
    }

    override func mouseMoved(with event: NSEvent) {
        if mpv != nil && mpv.getBoolProperty("input-cursor") {
            signalMouseMovement(event)
        }
        cocoaCB.titleBar.show()
    }

    override func mouseDragged(with event: NSEvent) {
        if mpv.getBoolProperty("input-cursor") {
            signalMouseMovement(event)
        }
    }

    override func mouseDown(with event: NSEvent) {
        if mpv.getBoolProperty("input-cursor") {
            signalMouseDown(event)
        }
    }

    override func mouseUp(with event: NSEvent) {
        if mpv.getBoolProperty("input-cursor") {
            signalMouseUp(event)
        }
        cocoaCB.window.isMoving = false
    }

    override func rightMouseDown(with event: NSEvent) {
        if mpv.getBoolProperty("input-cursor") {
            signalMouseDown(event)
        }
    }

    override func rightMouseUp(with event: NSEvent) {
        if mpv.getBoolProperty("input-cursor") {
            signalMouseUp(event)
        }
    }

    override func otherMouseDown(with event: NSEvent) {
        if mpv.getBoolProperty("input-cursor") {
            signalMouseDown(event)
        }
    }

    override func otherMouseUp(with event: NSEvent) {
        if mpv.getBoolProperty("input-cursor") {
            signalMouseUp(event)
        }
    }

    func signalMouseDown(_ event: NSEvent) {
        signalMouseEvent(event, SWIFT_KEY_STATE_DOWN)
        if event.clickCount > 1 {
            signalMouseEvent(event, SWIFT_KEY_STATE_UP)
        }
    }

    func signalMouseUp(_ event: NSEvent) {
        signalMouseEvent(event, SWIFT_KEY_STATE_UP)
    }

    func signalMouseEvent(_ event: NSEvent, _ state: Int32) {
        hasMouseDown = state == SWIFT_KEY_STATE_DOWN
        let mpkey = getMpvButton(event)
        cocoa_put_key_with_modifiers((mpkey | state), Int32(event.modifierFlags.rawValue));
    }

    func signalMouseMovement(_ event: NSEvent) {
        var point = convert(event.locationInWindow, from: nil)
        point = convertToBacking(point)
        point.y = -point.y

        cocoaCB.window.updateMovableBackground(point)
        if !cocoaCB.window.isMoving {
            mpv.setMousePosition(point)
        }
    }

    func preciseScroll(_ event: NSEvent) {
        var delta: Double
        var cmd: Int32

        if abs(event.deltaY) >= abs(event.deltaX) {
            delta = Double(event.deltaY) * 0.1;
            cmd = delta > 0 ? SWIFT_WHEEL_UP : SWIFT_WHEEL_DOWN;
        } else {
            delta = Double(event.deltaX) * 0.1;
            cmd = delta > 0 ? SWIFT_WHEEL_RIGHT : SWIFT_WHEEL_LEFT;
        }

        mpv.putAxis(cmd, delta: abs(delta))
    }

    override func scrollWheel(with event: NSEvent) {
        if !mpv.getBoolProperty("input-cursor") {
            return
        }

        if event.hasPreciseScrollingDeltas {
            preciseScroll(event)
        } else {
            let modifiers = event.modifierFlags
            let deltaX = modifiers.contains(.shift) ? event.scrollingDeltaY : event.scrollingDeltaX
            let deltaY = modifiers.contains(.shift) ? event.scrollingDeltaX : event.scrollingDeltaY
            var mpkey: Int32

            if abs(deltaY) >= abs(deltaX) {
                mpkey = deltaY > 0 ? SWIFT_WHEEL_UP : SWIFT_WHEEL_DOWN;
            } else {
                mpkey = deltaX > 0 ? SWIFT_WHEEL_RIGHT : SWIFT_WHEEL_LEFT;
            }

            cocoa_put_key_with_modifiers(mpkey, Int32(modifiers.rawValue))
        }
    }

    func containsMouseLocation() -> Bool {
        if cocoaCB == nil { return false }
        var topMargin: CGFloat = 0.0
        let menuBarHeight = NSApp.mainMenu!.menuBarHeight

        if cocoaCB.window.isInFullscreen && (menuBarHeight > 0) {
            topMargin = TitleBar.height + 1 + menuBarHeight
        }

        guard var vF = window?.screen?.frame else { return false }
        vF.size.height -= topMargin

        let vFW = window!.convertFromScreen(vF)
        let vFV = convert(vFW, from: nil)
        let pt = convert(window!.mouseLocationOutsideOfEventStream, from: nil)

        var clippedBounds = bounds.intersection(vFV)
        if !cocoaCB.window.isInFullscreen {
            clippedBounds.origin.y += TitleBar.height
            clippedBounds.size.height -= TitleBar.height
        }
        return clippedBounds.contains(pt)
    }

    func canHideCursor() -> Bool {
        if cocoaCB.window == nil { return false }
        return !hasMouseDown && containsMouseLocation() && window!.isKeyWindow
    }

    func getMpvButton(_ event: NSEvent) -> Int32 {
        let buttonNumber = event.buttonNumber
        switch (buttonNumber) {
            case 0:  return SWIFT_MBTN_LEFT;
            case 1:  return SWIFT_MBTN_RIGHT;
            case 2:  return SWIFT_MBTN_MID;
            case 3:  return SWIFT_MBTN_BACK;
            case 4:  return SWIFT_MBTN_FORWARD;
            default: return SWIFT_MBTN9 + Int32(buttonNumber - 5);
        }
    }
}

import QtQuick
import QtTest
import Milkshake

// The playback bar's pause, continue, next and stop controls, and the stop confirmation.
TestCase {
    id: tc
    name: "Transport"
    width: 1280
    height: 720
    visible: true
    when: windowShown

    Component { id: homeComponent; HomeScreen {} }
    Component { id: spyComponent; SignalSpy {} }

    property var home: null
    property var controller: null

    function init() {
        controller = harness.newController()
        home = createTemporaryObject(homeComponent, tc, {
            libraryFilter: harness.newFilter(), controller: controller, tags: harness.tags,
            previewDelay: 0, width: 1280, height: 720
        })
        verify(home !== null, "home screen created")
        waitForRendering(home)
    }

    function child(name) {
        const item = findChild(home, name)
        verify(item !== null, "found " + name)
        return item
    }
    function tap(name) {
        waitForRendering(home)
        mouseClick(child(name))
    }
    function spyOn(target, signalName) {
        return createTemporaryObject(spyComponent, tc, { target: target, signalName: signalName })
    }
    function startPlaying(id) {
        controller.playNow(id)
        waitForRendering(home)
    }

    // ---- pause and continue ----------------------------------------------------------------

    function test_pause_button_pauses_and_the_bar_says_so() {
        startPlaying("redline")
        compare(child("pauseButton").text, "pause")
        compare(child("transportLabel").text, "now playing")
        compare(child("pausedMark").visible, false)

        tap("pauseButton")
        compare(controller.paused, true)
        compare(child("pauseButton").text, "continue")
        compare(child("transportLabel").text, "paused")
        compare(child("pausedMark").visible, true)
        compare(child("playbackBar").visible, true)  // still there: there is a current item
        compare(child("nowPlayingTitle").text, "Redline")
    }

    function test_continue_button_resumes() {
        startPlaying("redline")
        tap("pauseButton")
        tap("pauseButton")
        compare(controller.paused, false)
        compare(child("pauseButton").text, "pause")
        compare(child("transportLabel").text, "now playing")
    }

    function test_next_while_paused_starts_the_next_collection_playing() {
        startPlaying("redline")
        controller.addToQueue("glass-garden")
        tap("pauseButton")
        tap("nextButton")
        compare(controller.paused, false)
        compare(controller.nowPlayingId, "glass-garden")
        compare(child("nowPlayingTitle").text, "Glass Garden")
    }

    function test_space_bar_toggles_pause_only_while_something_is_playing() {
        keyClick(Qt.Key_Space)  // idle: nothing to pause
        compare(controller.paused, false)
        compare(controller.playing, false)

        startPlaying("redline")
        keyClick(Qt.Key_Space)
        compare(controller.paused, true)
        keyClick(Qt.Key_Space)
        compare(controller.paused, false)
    }

    function test_swipe_view_while_paused_offers_queue_and_play_now_but_no_preview() {
        startPlaying("redline")
        tap("pauseButton")
        const previews = spyOn(controller, "previewRequested")
        tap("tile-glass-garden")  // an album, so it can be queued (the toggle would open interactive first)
        wait(60)
        compare(previews.count, 0)
        compare(child("playNowButton").visible, true)
        compare(child("addToQueueButton").visible, true)
        compare(child("playButton").visible, false)
    }

    // ---- stop ------------------------------------------------------------------------------

    function test_stop_asks_before_doing_anything() {
        startPlaying("redline")
        compare(child("stopDialog").visible, false)
        tap("stopButton")
        compare(child("stopDialog").visible, true)
        compare(child("dialogTitle").text, "stop playback?")
        compare(child("dialogMessage").text, "Stop Redline?")
        compare(controller.playing, true)  // nothing happened yet
    }

    function test_stop_message_mentions_the_queue_when_there_is_one() {
        startPlaying("redline")
        controller.addToQueue("glass-garden")
        tap("stopButton")
        compare(child("dialogMessage").text, "Stop Redline and clear the queue?")
    }

    function test_cancel_keeps_everything() {
        startPlaying("redline")
        controller.addToQueue("glass-garden")
        tap("stopButton")
        tap("dialogCancel")
        compare(child("stopDialog").visible, false)
        compare(controller.playing, true)
        compare(controller.queueLength, 1)
    }

    function test_tapping_outside_the_card_cancels() {
        startPlaying("redline")
        tap("stopButton")
        mouseClick(home, 20, 20)  // the dimmed area
        compare(child("stopDialog").visible, false)
        compare(controller.playing, true)
    }

    function test_tapping_the_card_itself_does_not_cancel() {
        startPlaying("redline")
        tap("stopButton")
        tap("dialogMessage")
        compare(child("stopDialog").visible, true)
    }

    function test_escape_cancels() {
        startPlaying("redline")
        tap("stopButton")
        keyClick(Qt.Key_Escape)
        compare(child("stopDialog").visible, false)
        compare(controller.playing, true)
    }

    function test_confirm_stops_and_clears_the_queue() {
        startPlaying("redline")
        controller.addToQueue("glass-garden")
        controller.addToQueue("after-hours")
        const stops = spyOn(controller, "stopped")
        tap("stopButton")
        tap("dialogConfirm")
        compare(child("stopDialog").visible, false)
        compare(controller.playing, false)
        compare(controller.queueLength, 0)
        compare(stops.count, 1)
        waitForRendering(home)
        compare(child("playbackBar").visible, false)
    }

    function test_stopping_while_paused_works() {
        startPlaying("redline")
        tap("pauseButton")
        tap("stopButton")
        tap("dialogConfirm")
        compare(controller.playing, false)
        compare(controller.paused, false)
    }

    function test_space_bar_does_nothing_while_the_dialog_is_open() {
        startPlaying("redline")
        tap("stopButton")
        keyClick(Qt.Key_Space)
        compare(controller.paused, false)
    }

    function test_the_dialog_closes_by_itself_if_playback_ends_meanwhile() {
        startPlaying("redline")
        tap("stopButton")
        compare(child("stopDialog").visible, true)
        controller.next()  // nothing queued: playback ends
        compare(child("stopDialog").visible, false)
    }
}

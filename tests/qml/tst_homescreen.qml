import QtQuick
import QtTest
import Milkshake

TestCase {
    id: tc
    name: "HomeScreen"
    width: 1280
    height: 720
    visible: true
    when: windowShown

    Component { id: homeComponent; HomeScreen {} }
    Component { id: spyComponent; SignalSpy {} }

    property var home: null
    property var controller: null
    property var filter: null

    function init() {
        controller = harness.newController()
        filter = harness.newFilter()
        home = createTemporaryObject(homeComponent, tc, {
            libraryFilter: filter, controller: controller, tags: harness.tags,
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
    // Layout updates on the next rendered frame, as it does between real taps.
    function tap(name) {
        waitForRendering(home)
        mouseClick(child(name))
    }
    function spyOn(target, signalName) {
        return createTemporaryObject(spyComponent, tc, { target: target, signalName: signalName })
    }
    function chooseType(value) {
        tap("typeDropdown")
        tap("typeDropdown-option-" + value)
    }
    function chooseTag(value) {
        tap("tagDropdown")
        tap("tagDropdown-option-" + value)
    }
    function openTile(id) {
        tap("tile-" + id)
        compare(home.view, "swipe")
    }

    // ---- grid ------------------------------------------------------------------------------

    function test_grid_shows_every_collection_first() {
        compare(home.view, "grid")
        compare(child("grid").count, 9)
        compare(child("grid").visible, true)
        compare(child("carouselView").visible, false)
    }

    function test_type_dropdown_filters_the_grid() {
        chooseType("playlist")
        compare(child("grid").count, 2)
        chooseType("interactive")
        compare(child("grid").count, 1)
        chooseType("all")
        compare(child("grid").count, 9)
    }

    function test_tag_dropdown_filters_the_grid() {
        chooseTag("edm")
        compare(child("grid").count, 4)
        chooseType("album")
        compare(child("grid").count, 3)
    }

    function test_only_one_dropdown_is_open_at_a_time() {
        tap("typeDropdown")
        compare(child("typeDropdown").open, true)
        tap("tagDropdown")
        compare(child("typeDropdown").open, false)
        compare(child("tagDropdown").open, true)
    }

    function test_tapping_outside_closes_a_dropdown() {
        tap("typeDropdown")
        compare(child("typeDropdown").open, true)
        mouseClick(home, 640, 500)
        compare(child("typeDropdown").open, false)
    }

    function test_no_matches_shows_an_empty_state_that_can_be_cleared() {
        chooseTag("challenging")
        chooseType("playlist")
        compare(child("grid").count, 1)  // club set
        chooseTag("pleasing")
        compare(child("grid").count, 1)  // sunday mix
        chooseType("interactive")
        compare(child("emptyState").visible, true)
        compare(child("grid").visible, false)
        tap("clearFilters")
        compare(child("emptyState").visible, false)
        compare(child("grid").count, 9)
    }

    // ---- swipe view ------------------------------------------------------------------------

    function test_tapping_a_tile_opens_it_in_the_swipe_view() {
        openTile("night-drive")
        compare(child("carouselView").visible, true)
        compare(child("grid").visible, false)
        compare(child("carouselView").currentId, "night-drive")
        compare(child("currentTitle").text, "Night Drive")
        compare(child("currentMeta").text, "album · 2 tracks · pleasing, edm")
    }

    function test_view_toggle_switches_between_the_two_views() {
        const toggle = child("viewToggle")
        mouseClick(toggle)
        compare(home.view, "swipe")
        mouseClick(toggle)
        compare(home.view, "grid")
    }

    function test_swiping_moves_to_the_next_and_previous_collection() {
        tap("viewToggle")
        const carousel = child("carouselView")
        compare(carousel.currentIndex, 0)
        compare(carousel.currentId, "interactive")
        mouseDrag(child("carousel"), 900, 150, -420, 0, Qt.LeftButton, Qt.NoModifier, 40)
        tryCompare(carousel, "currentIndex", 1)
        compare(carousel.currentId, "after-hours")
        mouseDrag(child("carousel"), 300, 150, 420, 0, Qt.LeftButton, Qt.NoModifier, 40)
        tryCompare(carousel, "currentIndex", 0)
    }

    function test_swipe_view_respects_the_filters() {
        chooseType("playlist")
        tap("viewToggle")
        compare(child("carouselView").currentId, "club-set")
        compare(child("carousel").count, 2)
    }

    // ---- previews and playback -------------------------------------------------------------

    function test_idle_swipe_view_offers_play_and_previews_the_collection() {
        const previews = spyOn(controller, "previewRequested")
        openTile("night-drive")
        tryCompare(previews, "count", 1)
        compare(previews.signalArguments[0][0], "night-drive")
        tryCompare(child("previewIndicator"), "visible", true)
        compare(child("playButton").visible, true)
        compare(child("playNowButton").visible, false)
        compare(child("addToQueueButton").visible, false)
    }

    function test_leaving_the_swipe_view_stops_the_preview() {
        const stops = spyOn(controller, "previewStopped")
        openTile("night-drive")
        tryCompare(child("previewIndicator"), "visible", true)
        tap("viewToggle")
        tryCompare(stops, "count", 1)
        compare(child("previewIndicator").visible, false)
    }

    function test_play_starts_playback_and_shows_the_playback_bar() {
        const plays = spyOn(controller, "playRequested")
        compare(child("playbackBar").visible, false)
        openTile("night-drive")
        tap("playButton")
        compare(plays.count, 1)
        compare(plays.signalArguments[0][0], "night-drive")
        compare(controller.playing, true)
        compare(child("playbackBar").visible, true)
        compare(child("nowPlayingTitle").text, "Night Drive")
    }

    function test_while_playing_the_swipe_view_offers_queue_and_play_now_and_no_preview() {
        openTile("night-drive")
        tap("playButton")
        tap("viewToggle")  // back to the grid
        const previews = spyOn(controller, "previewRequested")
        openTile("redline")
        wait(50)
        compare(previews.count, 0)
        compare(child("previewIndicator").visible, false)
        compare(child("playButton").visible, false)
        compare(child("playNowButton").visible, true)
        compare(child("addToQueueButton").visible, true)
    }

    function test_interactive_cannot_be_queued_only_played_now() {
        openTile("night-drive")
        tap("playButton")
        tap("viewToggle")
        openTile("interactive")
        compare(child("playNowButton").visible, true)
        compare(child("addToQueueButton").visible, false)
    }

    function test_add_to_queue_and_next_walk_the_queue() {
        openTile("night-drive")
        tap("playButton")
        tap("viewToggle")

        openTile("redline")
        tap("addToQueueButton")
        compare(controller.queueLength, 1)
        compare(child("queueLabel").text, "queue: 1")

        tap("nextButton")
        compare(controller.nowPlayingId, "redline")
        compare(child("nowPlayingTitle").text, "Redline")
        compare(controller.queueLength, 0)
    }

    function test_play_now_interrupts_and_keeps_the_queue() {
        openTile("night-drive")
        tap("playButton")
        tap("viewToggle")
        openTile("redline")
        tap("addToQueueButton")
        tap("viewToggle")
        openTile("after-hours")
        tap("playNowButton")
        compare(controller.nowPlayingId, "after-hours")
        compare(controller.queueLength, 1)
    }

    function test_previews_resume_when_playback_ends() {
        openTile("night-drive")
        tap("playButton")
        tap("viewToggle")
        openTile("redline")
        compare(child("previewIndicator").visible, false)
        const previews = spyOn(controller, "previewRequested")
        tap("nextButton")  // queue is empty: playback ends
        compare(controller.playing, false)
        tryCompare(previews, "count", 1)
        compare(previews.signalArguments[0][0], "redline")
        tryCompare(child("previewIndicator"), "visible", true)
    }
}

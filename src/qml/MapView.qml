import QtQuick
import QtQuick.Controls
import QtLocation
import QtPositioning

Item {
    id: root
    
    property var trackPoints: []
    property var photoMarkers: []
    property int highlightedIndex: -1
    
    // Map plugin (OpenStreetMap)
    Plugin {
        id: osmPlugin
        name: "osm"
        PluginParameter { name: "osm.mapping.highdpi_tiles"; value: "true" }
    }
    
    Map {
        id: map
        anchors.fill: parent
        plugin: osmPlugin
        center: QtPositioning.coordinate(20.0, 110.0)
        zoomLevel: 12
        
        // Enable pan and pinch gestures
        PinchHandler {
            id: pinch
            target: null
            onActiveChanged: if (active) {
                map.startCentroid = map.toCoordinate(pinch.centroid.position, false)
            }
            onScaleChanged: (delta) => {
                map.zoomLevel += Math.log2(delta)
                map.alignCoordinateToPoint(map.startCentroid, pinch.centroid.position)
            }
            onRotationChanged: (delta) => {
                map.bearing -= delta
                map.alignCoordinateToPoint(map.startCentroid, pinch.centroid.position)
            }
            grabPermissions: PointerHandler.TakeOverForbidden
        }
        
        WheelHandler {
            id: wheel
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            rotationScale: 1/120
            property: "zoomLevel"
        }

        DragHandler {
            id: drag
            target: null
            onTranslationChanged: (delta) => map.pan(-delta.x, -delta.y)
        }
        
        property geoCoordinate startCentroid
        
        // GPS Track polyline
        MapPolyline {
            id: trackLine
            line.width: 3
            line.color: "#3388ff"
            path: {
                var coords = [];
                for (var i = 0; i < root.trackPoints.length; i++) {
                    var pt = root.trackPoints[i];
                    coords.push(QtPositioning.coordinate(pt.latitude, pt.longitude));
                }
                return coords;
            }
        }
        
        // Photo markers
        MapItemView {
            model: ListModel { id: markerModel }
            delegate: MapQuickItem {
                coordinate: QtPositioning.coordinate(model.latitude, model.longitude)
                anchorPoint.x: markerIcon.width / 2
                anchorPoint.y: markerIcon.height
                
                sourceItem: Rectangle {
                    id: markerIcon
                    width: 24
                    height: 24
                    radius: 12
                    color: getStateColor(model.state)
                    border.width: model.index === root.highlightedIndex ? 3 : 1
                    border.color: model.index === root.highlightedIndex ? "#ff0" : "#fff"
                    
                    Text {
                        anchors.centerIn: parent
                        text: "📷"
                        font.pixelSize: 12
                    }
                    
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            mapPanel.photoMarkerClicked(model.index);
                        }
                    }
                    
                    // Tooltip
                    ToolTip {
                        id: tooltip
                        visible: mouseArea.containsMouse
                        text: model.fileName || ""
                    }
                    
                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: mapPanel.photoMarkerClicked(model.index)
                    }
                }
            }
        }
    }
    
    // Zoom controls
    Column {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 10
        spacing: 5
        
        Rectangle {
            width: 40
            height: 40
            radius: 5
            color: "#fff"
            border.color: "#ccc"
            
            Text {
                anchors.centerIn: parent
                text: "+"
                font.pixelSize: 24
            }
            
            MouseArea {
                anchors.fill: parent
                onClicked: map.zoomLevel += 1
            }
        }
        
        Rectangle {
            width: 40
            height: 40
            radius: 5
            color: "#fff"
            border.color: "#ccc"
            
            Text {
                anchors.centerIn: parent
                text: "−"
                font.pixelSize: 24
            }
            
            MouseArea {
                anchors.fill: parent
                onClicked: map.zoomLevel -= 1
            }
        }
        
        Rectangle {
            width: 40
            height: 40
            radius: 5
            color: "#fff"
            border.color: "#ccc"
            
            Text {
                anchors.centerIn: parent
                text: "⌖"
                font.pixelSize: 20
            }
            
            MouseArea {
                anchors.fill: parent
                onClicked: centerOnTrack()
            }
        }
    }
    
    // Helper function for state colors
    function getStateColor(state) {
        switch (state) {
            case 0: return "#888";    // Pending
            case 1: return "#44f";    // Processing
            case 2: return "#4a4";    // Success
            case 3: return "#aa4";    // Skipped
            case 4: return "#a44";    // Error
            default: return "#888";
        }
    }
    
    // JavaScript functions called from C++
    function setTrackPoints(points) {
        root.trackPoints = points;
        if (points.length > 0) {
            centerOnTrack();
        }
    }
    
    function addPhotoMarker(marker) {
        markerModel.append(marker);
    }
    
    function updatePhotoMarker(marker) {
        for (var i = 0; i < markerModel.count; i++) {
            if (markerModel.get(i).index === marker.index) {
                markerModel.set(i, marker);
                break;
            }
        }
    }
    
    function clearPhotoMarkers() {
        markerModel.clear();
    }
    
    function centerOnTrack() {
        if (root.trackPoints.length === 0) return;
        
        var minLat = 90, maxLat = -90;
        var minLon = 180, maxLon = -180;
        
        for (var i = 0; i < root.trackPoints.length; i++) {
            var pt = root.trackPoints[i];
            minLat = Math.min(minLat, pt.latitude);
            maxLat = Math.max(maxLat, pt.latitude);
            minLon = Math.min(minLon, pt.longitude);
            maxLon = Math.max(maxLon, pt.longitude);
        }
        
        var centerLat = (minLat + maxLat) / 2;
        var centerLon = (minLon + maxLon) / 2;
        map.center = QtPositioning.coordinate(centerLat, centerLon);
        
        // Adjust zoom to fit track
        var latSpan = maxLat - minLat;
        var lonSpan = maxLon - minLon;
        var span = Math.max(latSpan, lonSpan);
        
        if (span > 0.5) map.zoomLevel = 10;
        else if (span > 0.1) map.zoomLevel = 12;
        else if (span > 0.01) map.zoomLevel = 14;
        else map.zoomLevel = 16;
    }
    
    function highlightPhoto(index) {
        root.highlightedIndex = index;
        
        // Center on the photo
        for (var i = 0; i < markerModel.count; i++) {
            if (markerModel.get(i).index === index) {
                var marker = markerModel.get(i);
                map.center = QtPositioning.coordinate(marker.latitude, marker.longitude);
                break;
            }
        }
    }
}

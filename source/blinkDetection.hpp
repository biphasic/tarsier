#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace tarsier {

/// BlinkDetection detects blinks wow
template <typename Event, typename BlinkEvent, uint64_t lifespan,
          uint64_t lowerThreshold, uint64_t upperThreshold,
          typename BlinkEventFromEvent, typename HandleBlinkEvent>
class BlinkDetection {
public:
  BlinkDetection(BlinkEventFromEvent blinkEventFromEvent,
                 HandleBlinkEvent handleBlinkDetectionEvent)
      : _blinkEventFromEvent(
            std::forward<BlinkEventFromEvent>(blinkEventFromEvent)),
        _handleBlinkEvent(
            std::forward<HandleBlinkEvent>(handleBlinkDetectionEvent)),
        _lifespan(lifespan), _lowerThreshold(lowerThreshold),
        _upperThreshold(upperThreshold), _lastTimeStamp(0), _xGridSize(19),
        _yGridSize(20), _isBlink(false), _counter(0) {}

  BlinkDetection(const BlinkDetection &) = delete;
  BlinkDetection(BlinkDetection &&) = default;
  BlinkDetection &operator=(const BlinkDetection &) = delete;
  BlinkDetection &operator=(BlinkDetection &&) = default;
  virtual ~BlinkDetection() {}

  virtual void operator()(Event event) {
    if (event.timestamp > 500000 && event.x != 304 && event.y != 240) {
      std::cout << _counter++ << '\n';
      _xGridFocus = static_cast<int>(event.x / _xGridSize);
      _yGridFocus = static_cast<int>(event.y / _yGridSize);

      _currentTimeStamp = event.timestamp;
      _lastTimeStamp = _latestTimestampGrid[_xGridFocus][_yGridFocus];

      for (size_t i = 0; i <= 15; i++) {
        // tile that was activated
        if (i == _xGridFocus) {
          _activityGrid[i][_yGridFocus] *=
              exp(-static_cast<double>(_currentTimeStamp -
                                       _latestTimestampGrid[i][_yGridFocus]) /
                  _lifespan);
          _latestTimestampGrid[i][_yGridFocus] = _currentTimeStamp;
          // increase activity only in currently focused tile
          _activityGrid[i][_yGridFocus] += 1;
          continue;
        }

        // only update tiles that are beyond the lower threshold. Values below
        // are of no interest to an exponential decay
        if (_activityGrid[i][_yGridFocus] > _lowerThreshold) {
          _activityGrid[i][_yGridFocus] *=
              exp(-static_cast<double>(_currentTimeStamp -
                                       _latestTimestampGrid[i][_yGridFocus]) /
                  _lifespan);
          _latestTimestampGrid[i][_yGridFocus] = _currentTimeStamp;
        }

        if (_xGridFocus != i &&
            _lowerThreshold < _activityGrid[i][_yGridFocus] &&
            _activityGrid[i][_yGridFocus] < _upperThreshold) {
          otherBlink.push_back(i);
        }
      }

      // check whether it
      if (_lowerThreshold < _activityGrid[_xGridFocus][_yGridFocus] &&
          _activityGrid[_xGridFocus][_yGridFocus] < _upperThreshold &&
          otherBlink.size() == 1 &&
          abs((otherBlink.back() - _xGridFocus)) == 2) {
        _isBlink = true;
      } else {
        _isBlink = false;
      }
      otherBlink.clear();

      _handleBlinkEvent(_blinkEventFromEvent(event, _isBlink));
    }
  }

protected:
  BlinkEventFromEvent _blinkEventFromEvent;
  const uint64_t _lifespan;
  uint64_t _currentTimeStamp;
  uint64_t _lastTimeStamp;
  // double _activity;
  unsigned short int _xGridSize;
  unsigned short int _yGridSize;
  unsigned short int _xGridFocus;
  unsigned short int _yGridFocus;
  std::vector<unsigned short int> otherBlink;
  long _latestTimestampGrid[16][12];
  double _activityGrid[16][12];
  bool _blinkIndicatorGrid[16][12];
  long _blinkBeginTSGrid[16][12];
  bool _isBlink;
  uint64_t _lowerThreshold;
  uint64_t _upperThreshold;
  long _counter;
  HandleBlinkEvent _handleBlinkEvent;
};

template <typename Event, typename BlinkEvent, uint64_t lifespan,
          uint64_t lowerThreshold, uint64_t upperThreshold,
          typename BlinkEventFromEvent, typename HandleBlinkEvent>
BlinkDetection<Event, BlinkEvent, lifespan, lowerThreshold, upperThreshold,
               BlinkEventFromEvent, HandleBlinkEvent>
make_blinkDetection(BlinkEventFromEvent blinkEventFromEvent,
                    HandleBlinkEvent handleBlinkDetectionEvent) {
  return BlinkDetection<Event, BlinkEvent, lifespan, lowerThreshold,
                        upperThreshold, BlinkEventFromEvent, HandleBlinkEvent>(
      std::forward<BlinkEventFromEvent>(blinkEventFromEvent),
      std::forward<HandleBlinkEvent>(handleBlinkDetectionEvent));
}
}

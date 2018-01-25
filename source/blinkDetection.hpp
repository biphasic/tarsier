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
        _yGridSize(20), _isBlink(false) {}

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
      _lastTimeStamp = _grid[_xGridFocus][_yGridFocus].first;

      for (size_t i = 0; i <= 15; i++) {
        if (i == _xGridFocus) {
          _grid[i][_yGridFocus].second *=
              exp(-static_cast<double>(_currentTimeStamp -
                                       _grid[i][_yGridFocus].first) /
                  _lifespan);
          _grid[i][_yGridFocus].first = _currentTimeStamp;
          // increase activity only in currently focused tile
          _grid[i][_yGridFocus].second += 1;
          continue;
        }
        if (_grid[i][_yGridFocus].second > _lowerThreshold) {
          _grid[i][_yGridFocus].second *=
              exp(-static_cast<double>(_currentTimeStamp -
                                       _grid[i][_yGridFocus].first) /
                  _lifespan);
          _grid[i][_yGridFocus].first = _currentTimeStamp;
        }

        if (_xGridFocus != i &&
            _lowerThreshold < _grid[i][_yGridFocus].second &&
            _grid[i][_yGridFocus].second < _upperThreshold) {
          otherBlink.push_back(i);
        }
      }

      if (_lowerThreshold < _grid[_xGridFocus][_yGridFocus].second &&
          _grid[_xGridFocus][_yGridFocus].second < _upperThreshold &&
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
  std::pair<long, double> _grid[16][12];
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

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
      // std::cout << _counter++ << '\n';
      _col = static_cast<int>(event.x / _xGridSize);
      _row = static_cast<int>(event.y / _yGridSize);

      _currentTimeStamp = event.timestamp;
      _lastTimeStamp = _latestTimestampGrid[_col][_row];

      // updating activities
      for (size_t i = 0; i <= 15; i++) {
        // only update tiles that are beyond the lower threshold. Values below
        // are of no interest to an exponential decay
        if (_activityGrid[i][_row] > _lowerThreshold || i == _col) {
          _activityGrid[i][_row] *=
              exp(-static_cast<double>(_currentTimeStamp -
                                       _latestTimestampGrid[i][_row]) /
                  _lifespan);
          _latestTimestampGrid[i][_row] = _currentTimeStamp;
          if (i == _col) {
            // increase activity only in currently focused tile
            _activityGrid[i][_row] += 1;
          }

          // state machine
          switch (_blinkIndicatorGrid[i][_row]) {
          case 1:
            // see if activity has risen above lowerThreshold to set blink
            // candidate, state to 2
            if (_activityGrid[i][_row] > _lowerThreshold) {
              _blinkIndicatorGrid[i][_row] = 2;
              _blinkBeginTSGrid[i][_row] = _currentTimeStamp;
              std::cout << i << "/" << _row << "+" << '\n';
            } else if (_activityGrid[i][_row] > _upperThreshold) {
              std::cout << "This shouldn't have happened" << '\n';
            }
            break;
          case 2:
            // check whether activity may dropped below threshold plus some
            // safety margin
            if (_activityGrid[i][_row] < (_lowerThreshold - 15)) {
              std::cout << i << "/" << _row << "-" << '\n';
              _blinkIndicatorGrid[i][_row] = 1;
              _blinkVector.push_back(std::make_pair(
                  i, static_cast<long>((_latestTimestampGrid[i][_row] -
                                        _blinkBeginTSGrid[i][_row]) /
                                       2) +
                         _blinkBeginTSGrid[i][_row]));
            } else if (_activityGrid[i][_row] > _upperThreshold) {
              _blinkIndicatorGrid[i][_row] = 3; // considered clutter now
              std::cout << i << "/" << _row << "[++]" << '\n';
            }
            break;
          case 3:
            if (_activityGrid[i][_row] < _lowerThreshold) {
              _blinkIndicatorGrid[i][_row] = 1;
              std::cout << i << "/" << _row << "[--]" << '\n';
            }
            break;
          default:
            _blinkIndicatorGrid[i][_row] = 1;
            std::cout << "set state for first time. " << '\n';
            break;
          }
        }
      }

      // check whether central timestamps of the last two blinking candidates
      // (eyes) are not more than 50ms apart and candidates are two tiles apart
      // std::pair<int, long> previousBlink = *(_blinkVector.rbegin() + 1);
      if (_blinkVector.size() > 1 &&
          abs((_blinkVector.rbegin() + 1)->second -
              _blinkVector.back().second) < 50000 &&
          abs(((_blinkVector.rbegin() + 1)->first - _col)) == 2) {
        _isBlink = true;
        _blinkVector.clear();
      } else {
        _isBlink = false;
      }

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
  unsigned short int _col;
  unsigned short int _row;
  std::vector<unsigned short int> otherBlink;
  std::vector<std::pair<int, long>> _blinkVector;
  long _latestTimestampGrid[16][12];
  double _activityGrid[16][12];
  int _blinkIndicatorGrid[16][12];
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

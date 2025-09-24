import {useRef} from 'react';
import {PanResponder, GestureResponderEvent} from 'react-native';

export interface SwipeGestureConfig {
  onSwipeRight?: () => void;
  onSwipeLeft?: () => void;
  onSwipeUp?: () => void;
  onSwipeDown?: () => void;
  threshold?: number;
  velocityThreshold?: number;
}

export const useSwipeGesture = (config: SwipeGestureConfig) => {
  const {
    onSwipeRight,
    onSwipeLeft,
    onSwipeUp,
    onSwipeDown,
    threshold = 50,
    velocityThreshold = 500,
  } = config;

  const startX = useRef(0);
  const startY = useRef(0);
  const startTime = useRef(0);

  const panResponder = useRef(
    PanResponder.create({
      onStartShouldSetPanResponder: () => true,
      onMoveShouldSetPanResponder: () => true,
      onPanResponderGrant: (evt: GestureResponderEvent) => {
        startX.current = evt.nativeEvent.pageX;
        startY.current = evt.nativeEvent.pageY;
        startTime.current = Date.now();
      },
      onPanResponderRelease: (evt: GestureResponderEvent) => {
        const endX = evt.nativeEvent.pageX;
        const endY = evt.nativeEvent.pageY;
        const endTime = Date.now();

        const deltaX = endX - startX.current;
        const deltaY = endY - startY.current;
        const deltaTime = endTime - startTime.current;

        const absDeltaX = Math.abs(deltaX);
        const absDeltaY = Math.abs(deltaY);

        // Calculate velocity
        const velocityX = deltaTime > 0 ? absDeltaX / deltaTime : 0;
        const velocityY = deltaTime > 0 ? absDeltaY / deltaTime : 0;

        // Determine if it's a horizontal or vertical swipe
        if (absDeltaX > absDeltaY) {
          // Horizontal swipe
          if (absDeltaX > threshold || velocityX > velocityThreshold) {
            if (deltaX > 0 && onSwipeRight) {
              onSwipeRight();
            } else if (deltaX < 0 && onSwipeLeft) {
              onSwipeLeft();
            }
          }
        } else {
          // Vertical swipe
          if (absDeltaY > threshold || velocityY > velocityThreshold) {
            if (deltaY > 0 && onSwipeDown) {
              onSwipeDown();
            } else if (deltaY < 0 && onSwipeUp) {
              onSwipeUp();
            }
          }
        }
      },
    }),
  ).current;

  return {
    panResponder,
  };
};

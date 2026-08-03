import type { CurveAxis, OptionType, PriceRequest } from "./api";

export type ParamKey = Exclude<keyof PriceRequest, "type">;

export type SliderBound = {
  min: number;
  max: number;
  step: number;
};

export const DEFAULT_PARAMS: PriceRequest = {
  S0: 100,
  K: 100,
  r: 0.05,
  q: 0,
  sigma: 0.2,
  T: 1,
  type: "call" as OptionType,
};

export const DEFAULT_AXIS: CurveAxis = "S0";

export const DEFAULT_POINTS = 41;

export const SLIDER_BOUNDS: Record<ParamKey, SliderBound> = {
  S0: { min: 1, max: 500, step: 1 },
  K: { min: 1, max: 500, step: 1 },
  r: { min: 0, max: 0.3, step: 0.001 },
  q: { min: 0, max: 0.2, step: 0.001 },
  sigma: { min: 0.01, max: 1, step: 0.01 },
  T: { min: 0.01, max: 5, step: 0.01 },
};

export const PARAM_KEYS = Object.keys(SLIDER_BOUNDS) as ParamKey[];

export const AXIS_OPTIONS: CurveAxis[] = [
  "S0",
  "K",
  "r",
  "q",
  "sigma",
  "T",
];

export function getSliderBound(key: ParamKey): SliderBound {
  return SLIDER_BOUNDS[key];
}

export function clampParam(key: ParamKey, value: number): number {
  const { min, max, step } = SLIDER_BOUNDS[key];
  const clamped = Math.min(max, Math.max(min, value));
  const steps = Math.round((clamped - min) / step);
  return Number((min + steps * step).toFixed(10));
}

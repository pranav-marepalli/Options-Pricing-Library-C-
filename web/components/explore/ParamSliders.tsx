"use client";

import type { CurveAxis, OptionType, PriceRequest } from "@/lib/api";
import {
  AXIS_OPTIONS,
  PARAM_KEYS,
  type ParamKey,
  clampParam,
  getSliderBound,
} from "@/lib/explore-bounds";
import { Slider } from "@/components/ui/slider";

const PARAM_META: Record<
  ParamKey,
  { label: string; hint: string; format: (n: number) => string }
> = {
  S0: {
    label: "Spot",
    hint: "S₀",
    format: (n) => n.toFixed(0),
  },
  K: {
    label: "Strike",
    hint: "K",
    format: (n) => n.toFixed(0),
  },
  r: {
    label: "Rate",
    hint: "r",
    format: (n) => n.toFixed(3),
  },
  q: {
    label: "Dividend",
    hint: "q",
    format: (n) => n.toFixed(3),
  },
  sigma: {
    label: "Vol",
    hint: "σ",
    format: (n) => n.toFixed(2),
  },
  T: {
    label: "Tenor",
    hint: "T",
    format: (n) => n.toFixed(2),
  },
};

type ParamSlidersProps = {
  params: PriceRequest;
  axis: CurveAxis;
  onParamsChange: (next: PriceRequest) => void;
  onAxisChange: (axis: CurveAxis) => void;
};

export default function ParamSliders({
  params,
  axis,
  onParamsChange,
  onAxisChange,
}: ParamSlidersProps) {
  function setParam(key: ParamKey, value: number) {
    onParamsChange({ ...params, [key]: clampParam(key, value) });
  }

  function setType(type: OptionType) {
    onParamsChange({ ...params, type });
  }

  return (
    <div className="space-y-5">
      <div className="space-y-4">
        {PARAM_KEYS.map((key) => {
          const bound = getSliderBound(key);
          const meta = PARAM_META[key];
          const value = params[key];

          return (
            <label key={key} className="block">
              <span className="mb-2 flex items-baseline justify-between gap-2">
                <span className="flex items-baseline gap-2">
                  <span className="text-sm text-ink">{meta.label}</span>
                  <span className="font-mono text-[11px] text-muted">
                    {meta.hint}
                  </span>
                </span>
                <span className="font-mono text-sm tabular-nums text-accent">
                  {meta.format(value)}
                </span>
              </span>
              <Slider
                min={bound.min}
                max={bound.max}
                step={bound.step}
                value={[value]}
                onValueChange={([next]) => setParam(key, next)}
                aria-label={meta.label}
              />
            </label>
          );
        })}
      </div>

      <fieldset>
        <legend className="mb-1.5 text-sm text-ink">Type</legend>
        <div className="flex gap-2">
          {(["call", "put"] as const).map((t) => (
            <label
              key={t}
              className={`flex flex-1 cursor-pointer items-center justify-center border px-3 py-2.5 font-mono text-sm capitalize transition-colors ${
                params.type === t
                  ? "border-accent bg-accent-soft text-accent"
                  : "border-line bg-white/70 text-muted hover:border-accent/40 hover:text-ink"
              }`}
            >
              <input
                type="radio"
                name="explore-type"
                value={t}
                checked={params.type === t}
                onChange={() => setType(t)}
                className="sr-only"
              />
              {t}
            </label>
          ))}
        </div>
      </fieldset>

      <fieldset>
        <legend className="mb-1.5 text-sm text-ink">Sweep axis</legend>
        <div className="flex flex-wrap gap-1.5">
          {AXIS_OPTIONS.map((opt) => (
            <button
              key={opt}
              type="button"
              onClick={() => onAxisChange(opt)}
              className={`border px-2.5 py-1.5 font-mono text-xs tracking-wide transition-colors ${
                axis === opt
                  ? "border-accent bg-accent-soft text-accent"
                  : "border-line bg-white/70 text-muted hover:border-accent/40 hover:text-ink"
              }`}
            >
              {opt}
            </button>
          ))}
        </div>
      </fieldset>
    </div>
  );
}

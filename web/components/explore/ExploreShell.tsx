"use client";

import { useEffect, useRef, useState } from "react";

import {
  type CurveAxis,
  type CurveResponse,
  type PriceRequest,
  fetchCurve,
} from "@/lib/api";
import {
  DEFAULT_AXIS,
  DEFAULT_POINTS,
} from "@/lib/explore-bounds";
import CurveCharts from "@/components/explore/CurveCharts";
import GreeksPanel from "@/components/explore/GreeksPanel";
import ParamSliders from "@/components/explore/ParamSliders";

const DEBOUNCE_MS = 175;

type ExploreShellProps = {
  initialParams: PriceRequest;
  initialCurve: CurveResponse | null;
};

export default function ExploreShell({
  initialParams,
  initialCurve,
}: ExploreShellProps) {
  const [params, setParams] = useState<PriceRequest>(initialParams);
  const [axis, setAxis] = useState<CurveAxis>(DEFAULT_AXIS);
  const [curve, setCurve] = useState<CurveResponse | null>(initialCurve);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const skipFirstFetch = useRef(initialCurve !== null);

  useEffect(() => {
    if (skipFirstFetch.current) {
      skipFirstFetch.current = false;
      return;
    }

    const controller = new AbortController();
    const timer = window.setTimeout(async () => {
      setLoading(true);
      try {
        const data = await fetchCurve(
          { ...params, axis, points: DEFAULT_POINTS },
          { signal: controller.signal },
        );
        setCurve(data);
        setError(null);
      } catch (err) {
        if (controller.signal.aborted) return;
        setError(err instanceof Error ? err.message : "Request failed");
      } finally {
        if (!controller.signal.aborted) {
          setLoading(false);
        }
      }
    }, DEBOUNCE_MS);

    return () => {
      window.clearTimeout(timer);
      controller.abort();
    };
  }, [params, axis]);

  return (
    <div className="mx-auto w-full max-w-5xl px-5 py-14 sm:py-20">
      <header className="mb-10 animate-fade-up">
        <p className="font-mono text-xs tracking-[0.22em] text-accent uppercase">
          Options Pricing
        </p>
        <h1 className="mt-3 text-4xl leading-tight tracking-tight text-ink sm:text-5xl">
          Explore
        </h1>
        <p className="mt-3 max-w-xl text-base leading-relaxed text-muted">
          Sweep a parameter and watch price and Greeks move along the curve —
          live from the C++ API.
        </p>
      </header>

      <div
        className="grid gap-10 lg:grid-cols-[minmax(0,20rem)_minmax(0,1fr)] animate-fade-up"
        style={{ animationDelay: "60ms" }}
      >
        <aside className="space-y-8 border-t border-line pt-6 lg:border-t-0 lg:border-r lg:pr-8 lg:pt-0">
          <ParamSliders
            params={params}
            axis={axis}
            onParamsChange={setParams}
            onAxisChange={setAxis}
          />
        </aside>

        <div className="min-w-0 space-y-8">
          <div className="border-t border-line pt-6 lg:border-t-0 lg:pt-0">
            <GreeksPanel spot={curve?.spot ?? null} loading={loading} />
          </div>

          {error && (
            <p
              role="alert"
              className="border border-warn/30 bg-white/60 px-4 py-3 font-mono text-sm text-warn animate-fade-up"
            >
              {error}
            </p>
          )}

          <div className="border-t border-line pt-6">
            <CurveCharts curve={curve} params={params} />
          </div>
        </div>
      </div>
    </div>
  );
}

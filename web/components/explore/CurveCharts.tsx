"use client";

import {
  CartesianGrid,
  Legend,
  Line,
  LineChart,
  ReferenceLine,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from "recharts";

import type { CurveAxis, CurveResponse, PriceRequest } from "@/lib/api";

const ACCENT = "#1f6b4a";
const SECONDARY = "#4a5c50";
const GRID = "#c8d4cc";
const MUTED = "#4a5c50";

type CurveChartsProps = {
  curve: CurveResponse | null;
  params: PriceRequest;
};

type SeriesKey = "price" | "delta" | "gamma" | "vega" | "theta";

function axisLabel(axis: CurveAxis): string {
  return axis;
}

function formatTick(value: number): string {
  if (!Number.isFinite(value)) return "";
  const abs = Math.abs(value);
  if (abs >= 100) return value.toFixed(0);
  if (abs >= 1) return value.toFixed(2);
  return value.toFixed(3);
}

function ChartBlock({
  title,
  data,
  series,
  xLabel,
  referenceX,
}: {
  title: string;
  data: CurveResponse["series"];
  series: { key: SeriesKey; color: string; name: string }[];
  xLabel: string;
  referenceX: number | null;
}) {
  return (
    <div>
      <h3 className="mb-2 font-mono text-[11px] tracking-[0.18em] text-muted uppercase">
        {title}
      </h3>
      <div className="h-52 w-full sm:h-56">
        <ResponsiveContainer width="100%" height="100%">
          <LineChart
            data={data}
            margin={{ top: 8, right: 12, left: 0, bottom: 4 }}
          >
            <CartesianGrid stroke={GRID} strokeDasharray="3 3" vertical={false} />
            <XAxis
              dataKey="x"
              type="number"
              domain={["dataMin", "dataMax"]}
              tickFormatter={formatTick}
              tick={{ fill: MUTED, fontSize: 11, fontFamily: "var(--font-mono)" }}
              axisLine={{ stroke: GRID }}
              tickLine={{ stroke: GRID }}
              label={{
                value: xLabel,
                position: "insideBottomRight",
                offset: -2,
                style: {
                  fill: MUTED,
                  fontSize: 11,
                  fontFamily: "var(--font-mono)",
                },
              }}
            />
            <YAxis
              tickFormatter={formatTick}
              width={48}
              tick={{ fill: MUTED, fontSize: 11, fontFamily: "var(--font-mono)" }}
              axisLine={{ stroke: GRID }}
              tickLine={{ stroke: GRID }}
            />
            <Tooltip
              contentStyle={{
                background: "rgba(255,255,255,0.92)",
                border: `1px solid ${GRID}`,
                borderRadius: 0,
                fontFamily: "var(--font-mono)",
                fontSize: 12,
                color: "#0f1c14",
              }}
              labelFormatter={(label) =>
                `${xLabel} = ${formatTick(Number(label))}`
              }
              formatter={(value, name) => [
                formatTick(Number(value)),
                String(name),
              ]}
            />
            {series.length > 1 && (
              <Legend
                wrapperStyle={{
                  fontFamily: "var(--font-mono)",
                  fontSize: 11,
                  color: MUTED,
                }}
              />
            )}
            {referenceX != null && Number.isFinite(referenceX) && (
              <ReferenceLine
                x={referenceX}
                stroke={ACCENT}
                strokeDasharray="4 4"
                strokeOpacity={0.85}
              />
            )}
            {series.map(({ key, color, name }) => (
              <Line
                key={key}
                type="monotone"
                dataKey={key}
                name={name}
                stroke={color}
                strokeWidth={2}
                dot={false}
                isAnimationActive={false}
              />
            ))}
          </LineChart>
        </ResponsiveContainer>
      </div>
    </div>
  );
}

export default function CurveCharts({ curve, params }: CurveChartsProps) {
  if (!curve || curve.series.length === 0) {
    return (
      <p className="font-mono text-sm text-muted">
        No curve data yet. Adjust parameters to load a series.
      </p>
    );
  }

  const referenceX = params[curve.axis];
  const xLabel = axisLabel(curve.axis);

  return (
    <div className="space-y-8">
      <ChartBlock
        title="Price"
        data={curve.series}
        series={[{ key: "price", color: ACCENT, name: "Price" }]}
        xLabel={xLabel}
        referenceX={referenceX}
      />
      <ChartBlock
        title="Delta · Gamma"
        data={curve.series}
        series={[
          { key: "delta", color: ACCENT, name: "Delta" },
          { key: "gamma", color: SECONDARY, name: "Gamma" },
        ]}
        xLabel={xLabel}
        referenceX={referenceX}
      />
      <ChartBlock
        title="Vega · Theta"
        data={curve.series}
        series={[
          { key: "vega", color: ACCENT, name: "Vega" },
          { key: "theta", color: SECONDARY, name: "Theta" },
        ]}
        xLabel={xLabel}
        referenceX={referenceX}
      />
    </div>
  );
}

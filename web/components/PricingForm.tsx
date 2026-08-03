"use client";

import { FormEvent, useState } from "react";
import {
  fetchStoreLatest,
  impliedVol,
  OptionType,
  priceOption,
  PriceResponse,
  StoreEntry,
} from "@/lib/api";

type Mode = "price" | "implied-vol";

const defaults = {
  S0: "100",
  K: "100",
  r: "0.05",
  q: "0.0",
  sigma: "0.2",
  T: "1.0",
  type: "call" as OptionType,
  target: "10",
};

function fmt(n: number, digits = 6): string {
  if (!Number.isFinite(n)) return "—";
  return n.toLocaleString(undefined, {
    maximumFractionDigits: digits,
    minimumFractionDigits: 2,
  });
}

export default function PricingForm() {
  const [mode, setMode] = useState<Mode>("price");
  const [fields, setFields] = useState(defaults);
  const [result, setResult] = useState<PriceResponse | null>(null);
  const [iv, setIv] = useState<number | null>(null);
  const [storeEntry, setStoreEntry] = useState<StoreEntry | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);

  function update(key: keyof typeof defaults, value: string) {
    setFields((prev) => ({ ...prev, [key]: value }));
  }

  async function onSubmit(e: FormEvent) {
    e.preventDefault();
    setLoading(true);
    setError(null);
    setResult(null);
    setIv(null);
    setStoreEntry(null);

    const S0 = Number(fields.S0);
    const K = Number(fields.K);
    const r = Number(fields.r);
    const q = Number(fields.q);
    const sigma = Number(fields.sigma);
    const T = Number(fields.T);
    const target = Number(fields.target);

    try {
      if (mode === "price") {
        const data = await priceOption({
          S0,
          K,
          r,
          q,
          sigma,
          T,
          type: fields.type,
        });
        setResult(data);
      } else {
        const data = await impliedVol({
          S0,
          K,
          r,
          q,
          T,
          type: fields.type,
          target,
        });
        setIv(data.implied_vol);
        const priced = await priceOption({
          S0,
          K,
          r,
          q,
          sigma: data.implied_vol,
          T,
          type: fields.type,
        });
        setResult(priced);
      }
      // /v1/price upserts the shared hashmap store; confirm the latest entry.
      const latest = await fetchStoreLatest();
      setStoreEntry(latest.entry);
    } catch (err) {
      setError(err instanceof Error ? err.message : "Request failed");
    } finally {
      setLoading(false);
    }
  }

  const inputs: {
    key: keyof typeof defaults;
    label: string;
    hint: string;
    step?: string;
  }[] = [
    { key: "S0", label: "Spot", hint: "S₀", step: "any" },
    { key: "K", label: "Strike", hint: "K", step: "any" },
    { key: "r", label: "Rate", hint: "r", step: "any" },
    { key: "q", label: "Dividend", hint: "q", step: "any" },
    { key: "T", label: "Tenor", hint: "T (years)", step: "any" },
  ];

  return (
    <div className="mx-auto w-full max-w-xl px-5 py-14 sm:py-20">
      <header className="mb-10 animate-fade-up">
        <p className="font-mono text-xs tracking-[0.22em] text-accent uppercase">
          Options Pricing
        </p>
        <h1 className="mt-3 text-4xl leading-tight tracking-tight text-ink sm:text-5xl">
          Black–Scholes
        </h1>
        <p className="mt-3 max-w-md text-base leading-relaxed text-muted">
          Price European options and Greeks against the C++ API — or back out
          implied volatility from a market quote.
        </p>
      </header>

      <div
        className="mb-6 flex gap-1 border-b border-line animate-fade-up"
        style={{ animationDelay: "60ms" }}
        role="tablist"
        aria-label="Pricing mode"
      >
        {(
          [
            ["price", "Price + Greeks"],
            ["implied-vol", "Implied vol"],
          ] as const
        ).map(([id, label]) => (
          <button
            key={id}
            type="button"
            role="tab"
            aria-selected={mode === id}
            onClick={() => {
              setMode(id);
              setError(null);
              setResult(null);
              setIv(null);
            }}
            className={`-mb-px px-3 py-2 font-mono text-xs tracking-wide transition-colors ${
              mode === id
                ? "border-b-2 border-accent text-accent"
                : "border-b-2 border-transparent text-muted hover:text-ink"
            }`}
          >
            {label}
          </button>
        ))}
      </div>

      <form
        onSubmit={onSubmit}
        className="animate-fade-up space-y-5"
        style={{ animationDelay: "120ms" }}
      >
        <div className="grid grid-cols-2 gap-x-4 gap-y-4 sm:grid-cols-3">
          {inputs.map(({ key, label, hint, step }) => (
            <label key={key} className="block">
              <span className="mb-1.5 flex items-baseline justify-between gap-2">
                <span className="text-sm text-ink">{label}</span>
                <span className="font-mono text-[11px] text-muted">{hint}</span>
              </span>
              <input
                type="number"
                step={step}
                required
                value={fields[key]}
                onChange={(e) => update(key, e.target.value)}
                className="w-full border border-line bg-white/70 px-3 py-2.5 font-mono text-sm text-ink outline-none transition-[border-color,box-shadow] focus:border-accent focus:shadow-[0_0_0_3px_var(--accent-soft)]"
              />
            </label>
          ))}

          {mode === "price" ? (
            <label className="block">
              <span className="mb-1.5 flex items-baseline justify-between gap-2">
                <span className="text-sm text-ink">Vol</span>
                <span className="font-mono text-[11px] text-muted">σ</span>
              </span>
              <input
                type="number"
                step="any"
                required
                value={fields.sigma}
                onChange={(e) => update("sigma", e.target.value)}
                className="w-full border border-line bg-white/70 px-3 py-2.5 font-mono text-sm text-ink outline-none transition-[border-color,box-shadow] focus:border-accent focus:shadow-[0_0_0_3px_var(--accent-soft)]"
              />
            </label>
          ) : (
            <label className="block">
              <span className="mb-1.5 flex items-baseline justify-between gap-2">
                <span className="text-sm text-ink">Target</span>
                <span className="font-mono text-[11px] text-muted">price</span>
              </span>
              <input
                type="number"
                step="any"
                required
                value={fields.target}
                onChange={(e) => update("target", e.target.value)}
                className="w-full border border-line bg-white/70 px-3 py-2.5 font-mono text-sm text-ink outline-none transition-[border-color,box-shadow] focus:border-accent focus:shadow-[0_0_0_3px_var(--accent-soft)]"
              />
            </label>
          )}

          <fieldset className="col-span-2 sm:col-span-3">
            <legend className="mb-1.5 text-sm text-ink">Type</legend>
            <div className="flex gap-2">
              {(["call", "put"] as const).map((t) => (
                <label
                  key={t}
                  className={`flex flex-1 cursor-pointer items-center justify-center border px-3 py-2.5 font-mono text-sm capitalize transition-colors ${
                    fields.type === t
                      ? "border-accent bg-accent-soft text-accent"
                      : "border-line bg-white/70 text-muted hover:border-accent/40 hover:text-ink"
                  }`}
                >
                  <input
                    type="radio"
                    name="type"
                    value={t}
                    checked={fields.type === t}
                    onChange={() => update("type", t)}
                    className="sr-only"
                  />
                  {t}
                </label>
              ))}
            </div>
          </fieldset>
        </div>

        <button
          type="submit"
          disabled={loading}
          className="w-full bg-accent px-4 py-3 font-mono text-sm tracking-wide text-white transition-[transform,opacity] hover:opacity-90 active:scale-[0.99] disabled:cursor-wait disabled:opacity-60"
        >
          {loading
            ? "Computing…"
            : mode === "price"
              ? "Price option"
              : "Solve implied vol"}
        </button>
      </form>

      {error && (
        <p
          role="alert"
          className="mt-6 border border-warn/30 bg-white/60 px-4 py-3 font-mono text-sm text-warn animate-fade-up"
        >
          {error}
        </p>
      )}

      {(result || iv !== null) && (
        <section
          className="mt-8 border-t border-line pt-6 animate-fade-up"
          aria-live="polite"
        >
          <h2 className="font-mono text-xs tracking-[0.18em] text-muted uppercase">
            Results
          </h2>
          {iv !== null && (
            <p className="mt-4 font-mono text-2xl text-ink">
              σ = <span className="text-accent">{fmt(iv)}</span>
            </p>
          )}
          {result && (
            <dl className="mt-4 grid grid-cols-2 gap-x-6 gap-y-4 sm:grid-cols-3">
              {(
                [
                  ["Price", result.price],
                  ["Delta", result.delta],
                  ["Gamma", result.gamma],
                  ["Vega", result.vega],
                  ["Theta", result.theta],
                ] as const
              ).map(([label, value]) => (
                <div key={label}>
                  <dt className="font-mono text-[11px] tracking-wide text-muted uppercase">
                    {label}
                  </dt>
                  <dd className="mt-1 font-mono text-lg tabular-nums text-ink">
                    {fmt(value)}
                  </dd>
                </div>
              ))}
            </dl>
          )}
          {storeEntry && (
            <p className="mt-4 font-mono text-xs text-muted">
              Stored · version key{" "}
              <span className="text-accent">{storeEntry.key.slice(0, 12)}…</span>
              {" · "}
              price {fmt(storeEntry.price)} matches API store
            </p>
          )}
        </section>
      )}
    </div>
  );
}

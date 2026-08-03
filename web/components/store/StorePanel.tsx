"use client";

import { useCallback, useEffect, useState } from "react";

import {
  type StoreEntry,
  type StoreStatsResponse,
  fetchStoreLatest,
  fetchStoreRecent,
  fetchStoreStats,
  fetchStoreTop,
} from "@/lib/api";

function fmt(n: number, digits = 4): string {
  if (!Number.isFinite(n)) return "—";
  return n.toLocaleString(undefined, {
    maximumFractionDigits: digits,
    minimumFractionDigits: 2,
  });
}

function EntryRow({ entry }: { entry: StoreEntry }) {
  return (
    <li className="grid grid-cols-2 gap-x-4 gap-y-1 border-b border-line/60 py-3 font-mono text-sm last:border-b-0 sm:grid-cols-4">
      <span className="text-ink">
        {entry.type} · S₀={fmt(entry.S0, 2)} K={fmt(entry.K, 2)}
      </span>
      <span className="text-accent">price {fmt(entry.price)}</span>
      <span className="text-muted">δ {fmt(entry.delta)}</span>
      <span className="text-muted text-[11px]">
        {new Date(entry.ts_ms).toLocaleTimeString()}
      </span>
    </li>
  );
}

export default function StorePanel() {
  const [latest, setLatest] = useState<StoreEntry | null>(null);
  const [recent, setRecent] = useState<StoreEntry[]>([]);
  const [top, setTop] = useState<StoreEntry[]>([]);
  const [stats, setStats] = useState<StoreStatsResponse | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(true);

  const refresh = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const [latestRes, recentRes, topRes, statsRes] = await Promise.all([
        fetchStoreLatest(),
        fetchStoreRecent(10),
        fetchStoreTop(10),
        fetchStoreStats(),
      ]);
      setLatest(latestRes.entry);
      setRecent(recentRes.entries);
      setTop(topRes.entries);
      setStats(statsRes);
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed to load store");
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    void refresh();
    const id = setInterval(() => void refresh(), 4000);
    return () => clearInterval(id);
  }, [refresh]);

  return (
    <div className="mx-auto w-full max-w-3xl px-5 py-14 sm:py-20">
      <header className="mb-10 animate-fade-up">
        <p className="font-mono text-xs tracking-[0.22em] text-accent uppercase">
          Curve latency
        </p>
        <h1 className="mt-3 text-4xl leading-tight tracking-tight text-ink sm:text-5xl">
          Shared store
        </h1>
        <p className="mt-3 max-w-xl text-base leading-relaxed text-muted">
          Prices from the landing page upsert into a process-local hashmap with
          priority queues for latest and top-|price| retrieval. Curve responses
          use a short TTL cache.
        </p>
        <button
          type="button"
          onClick={() => void refresh()}
          className="mt-5 font-mono text-xs tracking-wide text-accent underline-offset-4 hover:underline"
        >
          {loading ? "Refreshing…" : "Refresh now"}
        </button>
      </header>

      {error && (
        <p
          role="alert"
          className="mb-6 border border-warn/30 bg-white/60 px-4 py-3 font-mono text-sm text-warn"
        >
          {error}
        </p>
      )}

      {stats && (
        <section className="mb-10 grid grid-cols-2 gap-4 border-t border-line pt-6 font-mono text-sm animate-fade-up sm:grid-cols-4">
          <div>
            <p className="text-[11px] tracking-wide text-muted uppercase">
              Store size
            </p>
            <p className="mt-1 text-lg text-ink">
              {stats.price_store.size}/{stats.price_store.max_entries}
            </p>
          </div>
          <div>
            <p className="text-[11px] tracking-wide text-muted uppercase">
              Version
            </p>
            <p className="mt-1 text-lg text-ink">{stats.price_store.version}</p>
          </div>
          <div>
            <p className="text-[11px] tracking-wide text-muted uppercase">
              Curve hits
            </p>
            <p className="mt-1 text-lg text-accent">{stats.curve_cache.hits}</p>
          </div>
          <div>
            <p className="text-[11px] tracking-wide text-muted uppercase">
              Curve misses
            </p>
            <p className="mt-1 text-lg text-ink">{stats.curve_cache.misses}</p>
          </div>
        </section>
      )}

      <section className="mb-10 animate-fade-up" style={{ animationDelay: "60ms" }}>
        <h2 className="font-mono text-xs tracking-[0.18em] text-muted uppercase">
          Latest (from Price option)
        </h2>
        {latest ? (
          <ul className="mt-3">
            <EntryRow entry={latest} />
          </ul>
        ) : (
          <p className="mt-3 font-mono text-sm text-muted">
            Empty — price an option on the home page to upsert.
          </p>
        )}
      </section>

      <section className="mb-10 animate-fade-up" style={{ animationDelay: "100ms" }}>
        <h2 className="font-mono text-xs tracking-[0.18em] text-muted uppercase">
          Recent
        </h2>
        <ul className="mt-3">
          {recent.map((e) => (
            <EntryRow key={`${e.key}-${e.ts_ms}`} entry={e} />
          ))}
          {recent.length === 0 && (
            <p className="font-mono text-sm text-muted">No recent entries.</p>
          )}
        </ul>
      </section>

      <section className="animate-fade-up" style={{ animationDelay: "140ms" }}>
        <h2 className="font-mono text-xs tracking-[0.18em] text-muted uppercase">
          Top |price|
        </h2>
        <ul className="mt-3">
          {top.map((e) => (
            <EntryRow key={`top-${e.key}-${e.ts_ms}`} entry={e} />
          ))}
          {top.length === 0 && (
            <p className="font-mono text-sm text-muted">No top entries.</p>
          )}
        </ul>
      </section>
    </div>
  );
}

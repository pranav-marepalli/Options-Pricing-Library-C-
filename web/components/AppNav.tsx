"use client";

import Link from "next/link";
import { usePathname } from "next/navigation";

const links = [
  { href: "/", label: "Price" },
  { href: "/explore", label: "Explore" },
  { href: "/curve-latency", label: "Store" },
] as const;

export default function AppNav() {
  const pathname = usePathname();

  return (
    <nav
      aria-label="Main"
      className="border-b border-line/80 bg-white/50 backdrop-blur-sm"
    >
      <div className="mx-auto flex w-full max-w-5xl items-center justify-between gap-4 px-5 py-3">
        <Link
          href="/"
          className="font-mono text-xs tracking-[0.18em] text-accent uppercase"
        >
          Options Pricing
        </Link>
        <ul className="flex items-center gap-1 sm:gap-2">
          {links.map(({ href, label }) => {
            const active =
              href === "/"
                ? pathname === "/"
                : pathname === href || pathname.startsWith(`${href}/`);
            return (
              <li key={href}>
                <Link
                  href={href}
                  className={`px-2.5 py-1.5 font-mono text-xs tracking-wide transition-colors sm:px-3 ${
                    active
                      ? "border-b-2 border-accent text-accent"
                      : "border-b-2 border-transparent text-muted hover:text-ink"
                  }`}
                >
                  {label}
                </Link>
              </li>
            );
          })}
        </ul>
      </div>
    </nav>
  );
}

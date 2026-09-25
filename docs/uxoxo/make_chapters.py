#!/usr/bin/env python3
import re, sys

def body_of(text):
    a = text.index(r'\begin{document}') + len(r'\begin{document}')
    b = text.index(r'\end{document}')
    return text[a:b]

def transform(src, out, chapter_title, notation, rename):
    txt = open(src).read()
    body = body_of(txt)
    # drop \maketitle and \tableofcontents lines
    lines = []
    for ln in body.splitlines():
        s = ln.strip()
        if s in (r'\maketitle', r'\tableofcontents', r'\appendix'):
            continue
        if s.startswith(r'\addcontentsline{toc}{section}{Part'):
            continue
        lines.append(ln)
    body = "\n".join(lines)
    # abstract -> styled chapter intro
    body = body.replace(r'\begin{abstract}', r'\begingroup\itshape\small')
    body = body.replace(r'\end{abstract}', r'\par\endgroup\medskip')
    # \part*{...} -> \uxpart{...}
    body = re.sub(r'\\part\*\{([^}]*)\}', r'\\uxpart{\1}', body)
    # insert \sectionnotation after each \section{...}\label{...} line
    out_lines = []
    sec_re = re.compile(r'^\\section\{.*\}\\label\{([^}]*)\}\s*$')
    for ln in body.splitlines():
        out_lines.append(ln)
        m = sec_re.match(ln.strip())
        if m:
            lab = m.group(1)
            if lab in notation:
                out_lines.append(r'\sectionnotation{%s}' % notation[lab])
    body = "\n".join(out_lines)
    # rename colliding labels (whole-token, safe for these distinctive keys)
    for old, new in rename.items():
        body = body.replace(old, new)
    header = ("\\documentclass[../uxoxo-main]{subfiles}\n"
              "\\begin{document}\n\n"
              "\\chapter{%s}\n" % chapter_title)
    open(out, "w").write(header + body + "\n\\end{document}\n")
    print("wrote", out)

# ---------------------------------------------------------------------
# Elements and Commands -- per-section "New in this section" content
# ---------------------------------------------------------------------
EC = {
 "sec:prelim":
   r"signature functor $F$; types $\Ttyp$, profile $\prof(\tau)$, state space $\St(\tau)$; "
   r"UI tree $\mu F$; free monad $\Free\Omega\,A$ (command) and cofree comonad $\Cofree F\,C$ (annotated tree); "
   r"Mealy handler $\handle_\tau$, events $\Ev$; option set $O$ with algebra $(\oplus,\sim,\operatorname{diff})$.",
 "sec:shared":
   r"the identification $\Omega=F$: one constructor read as UI \emph{type}, command \emph{keyword}, "
   r"\emph{opcode}, and parameter/child \emph{profile} (its arity).",
 "sec:cofree":
   r"the stateful UI tree $U=\mu X.(\St\times FX)$ with comonad operations $\extract,\duplicate,\extend$; "
   r"the response product $\widehat{\handle}_\tau$ and behavioural UI $\widehat U$; the \emph{element} as a Mealy node.",
 "sec:pairing":
   r"the located command $(p,e)$ and command program; the pairing $\run$ and the interpreter $\sem{-}$.",
 "sec:element":
   r"the \emph{UI element} $\delta=(\tau,\mathrm{attrs},\sigma,\widehat{\handle}_\tau)$ in four faces; "
   r"\emph{component} (composite element) and \emph{template} (a node of $\Free\Omega\,A$ with holes).",
 "sec:bijection":
   r"closed commands $\mathcal C^{0}=\Free\Omega\,\varnothing$; behavioural equivalence $\approx$; "
   r"the state-change monoid $\mathrm{Trans}(\widehat U)$.",
 "sec:attrs":
   r"attribute record $=$ command binding $=$ option set; $\setAttr,\setStyle$ as overlay $\oplus$; "
   r"the cascade (left $\oplus$-fold) and the reconciler $\operatorname{diff}_\pi$.",
 "sec:routing":
   r"routing as the \ty{Traversable} descent $\mathsf{traverse}$; dispatch as left-biased $\mathrm{alt}$ "
   r"(\ty{Alternative}); the capture and bubble phases.",
 "sec:render":
   r"rendering as the comonadic $\extend$; the serialization prism $\compose=\mathrm{cata}[\printfn]$ and "
   r"$\parsefn=\compose^{-1}$ on $\Lang$; the canonical command surface.",
 "sec:ladder":
   r"the expressiveness ladder $\FreeAp\subseteq\FreeSel\subseteq\Free$ (applicative $\subseteq$ selective "
   r"$\subseteq$ monadic).",
 "sec:learned":
   r"the capability-permitted sub-signature $\Omega_\kappa$; the glamour $g\in\Free\Omega_\kappa\,\varnothing$; "
   r"the agent (fairy) as the interpreter $\sem{-}$.",
 "sec:identity":
   r"the runtime state $\varrho=(U,\mathrm{globals})$ with pointer map $\mathrm{globals}:\{\dots\}\pfun N$; "
   r"the integrity constraint $\Psi$; identities $N\subseteq\II$.",
 "sec:mutation":
   r"update costs $\Theta(\mathrm{depth})$ (pure spine copy) and $\Theta(\mathrm{size})$ ($\extend$) vs.\ $O(1)$ "
   r"(imperative); the frequency$\times$locality parameter.",
 "sec:graphs":
   r"the sharing coalgebra $\Struct(F)$ (a finite DAG) and the cyclic/coalgebraic regime $\nu F$.",
 "sec:when":
   r"the effect monad $T(R)$; the value/schedule distinction (what a program computes vs.\ when).",
 "sec:strata":
   r"the three representation regimes: pure tree $\to$ $\Free/\Cofree$; sharing DAG $\to$ $\Struct(F)$; "
   r"identity/cycles/churn $\to$ imperative.",
}

# ---------------------------------------------------------------------
# The Expressible and Idealized UI -- per-section content
# ---------------------------------------------------------------------
IU = {
 "sec:prelim":
   r"the size bound $\mathsf B$ and the identity-isomorphism quotient; labelled transition system (LTS) and "
   r"its induced sub-LTS $(\mathcal S,\to)\!\restriction_\phi$.",
 "sec:workspaces":
   r"workspace $W=(N,r,p,\chi,\ell)$, document $\Doc(W)$, descendant orders $\dle,\dleq,\dlplus$; "
   r"well-formed set $\Tree$ and schema-valid $\Tree_\Sigma$; element $\delta=(\tau,A,S,E,\rho)$, "
   r"state algebra $\St(\tau)$, region $\reg$; atomic vs.\ composite.",
 "sec:authoring":
   r"the editor LTS $\Edit$; primitive operations $\Ops_\top=\{\mint,\attach,\detach,\delete,\relabel\}$ and "
   r"derived $\move$; the authoring relation $\step{}$.",
 "sec:runtime":
   r"runtime state $\varrho$ and its space $\mathrm{RT}(W)$; the no-dangling invariant $\Psi$; "
   r"$\hit,\route,\handle$ and the runtime relation $\runstep{}$.",
 "sec:uistate":
   r"the UI state $s=(W,\varrho)$ and the (finite) UI-state space $\Ubd$.",
 "sec:gc":
   r"generative completeness (structural $+$ behavioural) and \emph{expressibility}; initial state $s_0(\tau)$; "
   r"the reachable set $\Reach(\Wempty,\step{}_\Sigma)$.",
 "sec:edit":
   r"the mode $m\in\{\modeUse,\modeEdit\}$; the capability set $\kappa(n)$; the mode escape.",
 "sec:faces":
   r"the regular tree grammar $G_\Sigma$ with $L(G_\Sigma)$; the instruction algebra (State-monad actions); "
   r"decidability of the finite system.",
 "sec:ctx":
   r"the context projection $\proj:\Ubd\twoheadrightarrow\Ctx$ and its fibers $\Ufib{x}$.",
 "sec:pref":
   r"the preference structure $\{\succeq_x\}_{x}$ (a total preorder per fiber) and the utility $U_\uu$.",
 "sec:ideal":
   r"the ideal policy $\pi^\star_\uu$ and the optimal transversal $\Uopt=\img(\pi^\star_\uu)$.",
 "sec:lattice":
   r"invariants as the lattice $2^{\Ubd}$; the induced sub-LTS $\mathsf S(\phi)$ and the meet-semilattice "
   r"homomorphism.",
 "sec:phiu":
   r"the preference invariant $\Phiu$ and the personalized configuration.",
 "sec:obs":
   r"actions $a$ (interactions and edits); the Boltzmann/Gibbs behavioural law $\law_\theta(a\mid s)$ with "
   r"rationality $\beta$.",
 "sec:bayes":
   r"the finite hypothesis class $\Theta$ and prior $\mu_0$; the posterior (usage model) $\mu_t$, the Bayes "
   r"operator $\learn$, and the MAP estimate $\hat\theta_t$.",
 "sec:consistency":
   r"realizability and the ideal weights $\theta^\star$; the alignment operator $\Lambda$; the divergence $\KL$.",
 "sec:map":
   r"the fiber Gibbs law $\Glaw_\theta(s\mid x)$ and the MAP policy $\hat\pi_\theta$.",
 "sec:agent":
   r"the glamour and grimoire; the value-driven agent $F$ and its value function $U$; the idealized UI tuple.",
 "sec:loop":
   r"the closed loop with confidence gate $\epsilon$; the installed hypothesis and the non-thrashing argument.",
 "sec:limits":
   r"the three idealizing hypotheses recalled: coherence (H1), realizability (H2), identifiability$+$exploration (H3).",
}

IU_RENAME = {
 "sec:prelim": "iu:prelim",
 "sec:synth": "iu:synth",
 "app:crosswalk": "iu:crosswalk",
 "def:element": "iu:elt",
}
# (notation is looked up BEFORE rename, so IU keys stay as original labels)

transform("/home/claude/synth/elements-and-commands.tex",
          "/home/claude/uxoxo/chapters/elements-and-commands.tex",
          r"Elements and Commands: the UI--DSL Correspondence",
          EC, {})

# For IU, rename happens last inside transform(); but notation is keyed on the
# ORIGINAL label except prelim. Apply rename map; key sec:prelim already moved.
transform("/mnt/user-data/outputs/idealized-ui.tex",
          "/home/claude/uxoxo/chapters/idealized-ui.tex",
          r"The Expressible and the Idealized UI",
          IU, IU_RENAME)

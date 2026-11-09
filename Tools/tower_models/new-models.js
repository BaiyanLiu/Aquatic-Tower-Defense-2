// Five aquatic tower proposals, in the same shape language and pivot names as gm-towers.js.
// +X is the aim direction, Z is up, the origin is the top of the stone platform.
function curve(pts) {
  return (u) => {
    const n = pts.length - 1, f = Math.min(Math.max(u, 0) * n, n - 1e-6), i = Math.floor(f), t = f - i;
    const at = (k) => pts[Math.max(0, Math.min(n, k))];
    return [0, 1, 2].map(a => {
      const p0 = at(i - 1)[a], p1 = at(i)[a], p2 = at(i + 1)[a], p3 = at(i + 2)[a];
      return 0.5 * (2 * p1 + (p2 - p0) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (3 * p1 - p0 - 3 * p2 + p3) * t * t * t);
    });
  };
}
function polyline(parent, f, n, d, c, o) { let prev = f(0); for (let k = 1; k <= n; k++) { const q = f(k / n); tube(parent, prev, q, typeof d === "function" ? d(k / n) : d, c, o); prev = q; } }
function rockRing(root, n, r, z, seed, s0 = 18) { for (let i = 0; i < n; i++) { const an = i / n * Math.PI * 2 + seed, s = s0 + (i % 3) * 5; S(root, "sphere", [Math.cos(an) * r, Math.sin(an) * r, z + (i % 2) * 3], [s + 5, s, s * 0.8], ROCK[i % 4], [0, an / D2R, 0], MATTE); } }
function brokenShell(parent, p, s, yaw, c = [0.85, 0.42, 0.2]) {
  S(parent, "sphere", p, [s, s * 1.2, s * 0.5], c, [14, yaw, 8]);
  S(parent, "sphere", [p[0] + s * 0.2, p[1], p[2] + s * 0.1], [s * 0.3, s * 0.3, s * 0.32], [0.05, 0.05, 0.06]);
  S(parent, "cone", [p[0] - s * 0.4, p[1] + s * 0.4, p[2] + s * 0.1], [s * 0.2, s * 0.2, s * 0.4], c, { dir: [-0.6, 0.6, 0.4] });
}
function pulseRing(parent, n, r, z, d, m) { const g = P(parent, [0, 0, z]); around(n, r, 0, (x, y) => S(g, "sphere", [x, y, 0], [d, d, d], 0, null, { material: m })); return g; }

const PORTING = typeof PORT !== "undefined";
const fx = (o) => { o.userData.fx = true; return o; }; // preview-only effect, left out of the game model

const IDEAS = {
  shotLionfish(root) {
    S(root, "cylinder", [-2, 0, 0], [3.2, 3.2, 30], C.bone, { dir: [1, 0, 0] });
    for (const x of [-12, -6, 0, 6]) S(root, "cylinder", [x, 0, 0], [3.8, 3.8, 2.4], [0.55, 0.07, 0.06], { dir: [1, 0, 0] });
    S(root, "cone", [17, 0, 0], [7, 7, 12], [0.45, 1, 0.3], { dir: [1, 0, 0] }, { emissive: [0.25, 0.85, 0.12], tip: true });
    S(root, "sphere", [-17, 0, 0], [7, 7, 7], [0.55, 0.07, 0.06]);
  },
  shotSawfish(root) {
    for (let k = 0; k < 3; k++) { const an = k / 3 * Math.PI * 2; S(root, "cone", [Math.cos(an) * 8, Math.sin(an) * 8, 0], [7, 3.2, 18], [0.97, 0.94, 0.82], { dir: [Math.cos(an), Math.sin(an), 0] }, { rough: 0.2, tip: true }); }
    S(root, "cylinder", [0, 0, 0], [8, 8, 3.2], [0.75, 0.05, 0.05]);
  },
  shotLionfish(root) {
    S(root, "cylinder", [-2, 0, 0], [3.2, 3.2, 30], C.bone, { dir: [1, 0, 0] });
    for (const x of [-12, -6, 0, 6]) S(root, "cylinder", [x, 0, 0], [3.8, 3.8, 2.4], [0.55, 0.07, 0.06], { dir: [1, 0, 0] });
    S(root, "cone", [17, 0, 0], [7, 7, 12], [0.45, 1, 0.3], { dir: [1, 0, 0] }, { emissive: [0.25, 0.85, 0.12], tip: true });
    S(root, "sphere", [-17, 0, 0], [7, 7, 7], [0.55, 0.07, 0.06]);
  },
  shotSawfish(root) {
    for (let k = 0; k < 3; k++) { const an = k / 3 * Math.PI * 2; S(root, "cone", [Math.cos(an) * 8, Math.sin(an) * 8, 0], [7, 3.2, 18], [0.97, 0.94, 0.82], { dir: [Math.cos(an), Math.sin(an), 0] }, { rough: 0.2, tip: true }); }
    S(root, "cylinder", [0, 0, 0], [8, 8, 3.2], [0.75, 0.05, 0.05]);
  },
  // ------------------------------------------------------------------ Rock Thrower
  octopus(root) {
    const skin = [0.84, 0.17, 0.05], skinD = [0.45, 0.07, 0.05], skinL = [0.96, 0.3, 0.1], sucker = [1, 0.84, 0.72], sand = [0.72, 0.62, 0.42];
    const stone = [[0.5, 0.51, 0.53], [0.42, 0.43, 0.46], [0.58, 0.56, 0.52]];
    const R = rng(41);
    // the octopus's midden: a sand bed heaped with shells and sorted throwing stones
    S(root, "sphere", [0, 0, 5], [88, 88, 18], sand, null, MATTE);
    for (let i = 0; i < 10; i++) { const an = i / 10 * 6.283 + 0.2; S(root, "sphere", [Math.cos(an) * 38, Math.sin(an) * 38, 4], [15, 12, 6], i % 2 ? [0.6, 0.52, 0.34] : [0.68, 0.58, 0.38], null, MATTE); }
    const scallop = (p, s, yaw, c) => {
      const g = P(root, p); g.quaternion.copy(rotQ([-24, yaw, 0]));
      S(g, "sphere", [0, 0, 0], [s, s * 1.05, s * 0.3], c);
      for (let k = 0; k < 7; k++) { const a = (k / 6 - 0.5) * 2.1; tube(g, [-s * 0.38, 0, s * 0.1], [Math.cos(a) * s * 0.46, Math.sin(a) * s * 0.46, s * 0.1], 1.1, c.map(v => v * 0.72)); }
      S(g, "cube", [-s * 0.42, 0, 0], [s * 0.2, s * 0.42, 1.8], c.map(v => v * 0.85));
    };
    const shellC = [[0.98, 0.76, 0.66], [0.96, 0.9, 0.78], [0.82, 0.55, 0.72], [0.98, 0.62, 0.42], [0.7, 0.82, 0.9]];
    for (let i = 0; i < 9; i++) { const an = i * 0.72 + 2.1, r = 28 + (i % 3) * 7; scallop([Math.cos(an) * r, Math.sin(an) * r, 12 + (i % 2) * 2], 13 + (i % 3) * 2.5, an / D2R + 150, shellC[i % 5]); }
    // sorted pile of throwing stones, close at hand
    for (let i = 0; i < 12; i++) { const d = fib(i, 12); S(root, "sphere", [-30 + d[0] * 9, -24 + d[1] * 9, 13 + Math.abs(d[2]) * 7], [8, 8, 7], stone[i % 3]); }
    for (let i = 0; i < 7; i++) { const an = R() * 6.28, r = 20 + R() * 20; S(root, "sphere", [Math.cos(an) * r, Math.sin(an) * r, 12 + R() * 2], [5, 5, 4], stone[i % 3]); }

    const a = P(root, [0, 0, 8], "Anim"); P(a, [46, 0, 62], "Muzzle");
    // a tapering arm: skin in overlapping spheres, a pale sucker row along the underside
    const arm = (parent, pts, N, w0, w1) => {
      const f = curve(pts);
      for (let k = 0; k < N; k++) {
        const u = k / (N - 1), p = f(u), w = w0 + (w1 - w0) * u, q = f(Math.min(1, u + 0.04)), o = f(Math.max(0, u - 0.04));
        S(parent, "sphere", p, [w, w, w], skin);
        const tx = q[0] - o[0], ty = q[1] - o[1], tz = q[2] - o[2], L = Math.hypot(tx, ty, tz) || 1;
        let nx = -tz * tx / L / L, ny = -tz * ty / L / L, nz = 1 - tz * tz / L / L;
        const nL = Math.hypot(nx, ny, nz) || 1; nx /= -nL; ny /= -nL; nz /= -nL;
        if (k % 3 === 0) S(parent, "sphere", [p[0] + nx * w * 0.42, p[1] + ny * w * 0.42, p[2] + nz * w * 0.42], [w * 0.42, w * 0.42, w * 0.42], sucker);
      }
    };
    // six arms sprawled over the midden, curling at the tips
    const sway = [];
    for (let i = 0; i < 6; i++) {
      const an = (i < 3 ? 1.05 + i * 0.75 : 3.0 + (i - 3) * 0.75 + 0.65), c = Math.cos(an), s = Math.sin(an), side = i % 2 ? 1 : -1;
      const g = P(a, [c * 12, s * 12, 18], "Tentacle");
      arm(g, [[0, 0, 0], [c * 14, s * 14, -8], [c * 26, s * 26, -11], [c * 34 - s * side * 3, s * 34 + c * side * 3, -6], [c * 34 - s * side * 11, s * 34 + c * side * 11, -1], [c * 28 - s * side * 12, s * 28 + c * side * 12, 2]], 26, 11, 2.8);
      sway.push(g);
    }
    // bulbous mantle tipped back, with warts and mottling
    S(a, "sphere", [-12, 0, 50], [50, 46, 58], skin);
    S(a, "sphere", [-16, 0, 66], [34, 32, 26], skinL);
    S(a, "sphere", [6, 0, 28], [38, 38, 30], skin);
    for (let i = 0; i < 30; i++) {
      const d = fib(i, 30); if (d[0] > 0.72 && Math.abs(d[2]) < 0.4) continue;
      const p = [-12 + d[0] * 24.5, d[1] * 22.5, 50 + d[2] * 28.5];
      if (i % 3 === 0) S(a, "sphere", p, [8, 7, 4], skinD, { dir: d }); else S(a, "sphere", p, [4.5, 4.5, 3], skinL, { dir: d });
    }
    // eyes on raised turrets: gold with a flat bar pupil
    for (const y of [-1, 1]) {
      S(a, "sphere", [16, y * 15, 38], [15, 13, 15], skin);
      S(a, "sphere", [20, y * 16.5, 39], [11, 9, 11], [0.98, 0.78, 0.2], null, GLOSS);
      S(a, "sphere", [24.6, y * 18, 39], [2.2, 5.5, 2.6], C.black, null, GLOSS);
      S(a, "sphere", [24, y * 19, 41.6], [1.8, 1.8, 1.8], C.white, null, { emissive: [0.7, 0.7, 0.7] });
      S(a, "cone", [13, y * 15, 47.5], [6, 4, 8], skinD, { dir: [0.1, 0, 1] });
    }
    tube(a, [20, -16, 24], [30, -22, 26], 7, skinD); S(a, "cylinder", [31, -22.6, 26], [8, 8, 2.5], skinD, { dir: [10, -6, 2] });
    // two raised throwing arms, each cocked behind the head with a stone
    const throwers = [];
    for (const y of [-1, 1]) {
      const g = P(a, [12, y * 19, 24], "Thrower");
      arm(g, [[0, 0, 0], [9, y * 7, 16], [8, y * 11, 34], [-3, y * 11, 48], [-13, y * 8, 55]], 26, 10.5, 3.4);
      const ston = S(g, "sphere", [-17, y * 8, 58], [11, 11, 10], stone[y > 0 ? 2 : 0]);
      throwers.push({ g, ston, y });
    }
    // preview shots: stones and the odd shell on a shallow arc
    const shots = []; for (let i = 0; i < 6; i++) { const q = fx(S(a, "sphere", [0, 0, 0], [8, 8, 7], stone[i % 3])); shots.push(q); }
    return (t) => {
      sweep(a, t, 0.32);
      a.position.z = 8 + Math.sin(t * 1.6) * 1.2;
      sway.forEach((g, i) => { g.rotation.x = Math.sin(t * 1.6 + i * 1.1) * 0.05; g.rotation.y = Math.cos(t * 1.3 + i) * 0.05; });
      // the two arms alternate, which is what makes it so fast
      throwers.forEach(({ g, ston }, i) => {
        const ph = ((t * 0.8 + i * 0.5) % 1), w = ph < 0.12 ? ph / 0.12 : ph < 0.45 ? 1 - (ph - 0.12) / 0.33 : 0;
        g.rotation.y = w * 1.25; ston.visible = ph > 0.4 || ph < 0.08;
      });
      shots.forEach((q, i) => {
        const u = (t * 1.6 + i / 6) % 1, y = (i % 2 ? 1 : -1) * 18;
        q.position.set(30 + u * 120, y * (1 - u * 0.6), 56 + u * 12 - u * u * 34); q.visible = u < 0.9;
      });
    };
  },
  // ------------------------------------------------------------------ Spear Thrower
  mantis(root) {
    const teal = [0.04, 0.58, 0.44], tealL = [0.1, 0.78, 0.55], tealD = [0.02, 0.32, 0.34], blue = [0.1, 0.32, 0.85], orange = [1, 0.42, 0.08], red = [0.92, 0.1, 0.12], gold = [1, 0.8, 0.22], cream = [0.96, 0.9, 0.72];
    // burrow mound with crushed crab shells, the tower's whole personality
    S(root, "sphere", [0, 0, 3], [84, 84, 14], [0.55, 0.47, 0.34], null, MATTE);
    rockRing(root, 10, 31, 8, 0.2);
    S(root, "cylinder", [-8, 0, 10], [38, 38, 3], C.black);
    const R = rng(9);
    for (let i = 0; i < 5; i++) { const an = 0.5 + i * 1.25; brokenShell(root, [Math.cos(an) * 35, Math.sin(an) * 35, 18 + (i % 2) * 2], 13 + (i % 3) * 2, an / D2R); }
    for (let i = 0; i < 12; i++) { const an = R() * 6.28, r = 26 + R() * 16; S(root, "cone", [Math.cos(an) * r, Math.sin(an) * r, 12 + R() * 4], [5, 4, 5], [0.9, 0.6, 0.35], { dir: [R() - 0.5, R() - 0.5, 0.6] }); }
    for (let i = 0; i < 6; i++) { const an = R() * 6.28; barnacle(root, [Math.cos(an) * 34, Math.sin(an) * 34, 16], 4 + R() * 2, [Math.cos(an), Math.sin(an), 0.5]); }

    const a = P(root, [0, 0, 8], "Anim"); P(a, [62, 0, 30], "Muzzle");
    // segmented abdomen arching from the tail fan up into the thorax
    const spine = curve([[-38, 0, 10], [-28, 0, 15], [-14, 0, 24], [0, 0, 36], [12, 0, 48]]);
    const N = 9;
    for (let i = 0; i < N; i++) {
      const u = i / (N - 1), p = spine(u), q = spine(Math.min(1, u + 0.05)), o = spine(Math.max(0, u - 0.05));
      const dir = [q[0] - o[0], q[1] - o[1], q[2] - o[2]], w = 19 + u * 13;
      S(a, "sphere", p, [w, w * 0.92, 12], i % 2 ? teal : tealD, { dir }, GLOSS);
      S(a, "sphere", [p[0] + dir[2] * 0.02, 0, p[2] - w * 0.24], [w * 0.74, w * 0.7, 9], [0.92, 0.5, 0.32], { dir });
      S(a, "cylinder", p, [w * 1.03, w * 0.95, 1.6], blue, { dir });
      for (const y of [-1, 1]) {
        S(a, "sphere", [p[0], y * w * 0.42, p[2] + w * 0.12], [4.5, 2.5, 4.5], gold, null, GLOSS);
        S(a, "cone", [p[0], y * w * 0.48, p[2] - w * 0.1], [3, 3, 7], red, { dir: [0, y, -0.4] });
        if (i < 6) tube(a, [p[0], y * w * 0.3, p[2] - w * 0.3], [p[0] + 3, y * (w * 0.42), p[2] - w * 0.3 - 8], 3, orange);
      }
      S(a, "cone", [p[0] - 1, 0, p[2] + w * 0.44], [4, 3, 7], red, { dir: [-0.3, 0, 1] });
    }
    // tail fan: telson plus paddles with blue edges and orange spines
    const tail = P(a, [-40, 0, 9], "Tail");
    for (let k = 0; k < 6; k++) {
      const an = (k / 5 - 0.5) * 2.2, d = [-Math.cos(an), Math.sin(an), 0.15];
      S(tail, "sphere", [d[0] * 12, d[1] * 12, 1], [16, 6, 24], k % 2 ? orange : blue, { dir: d }, GLOSS);
      S(tail, "sphere", [d[0] * 21, d[1] * 21, 2.4], [5, 3, 5], gold);
    }
    S(tail, "sphere", [-8, 0, 2], [20, 11, 5], teal, null, GLOSS);
    for (const y of [-3, 0, 3]) S(tail, "cone", [-19, y, 2.4], [3, 3, 7], red, { dir: [-1, y * 0.1, 0] });
    // walking legs braced on the mound
    for (const y of [-1, 1]) for (let k = 0; k < 3; k++) {
      const x = 8 + k * 5 - 8, z = 40 - k * 4;
      polyline(a, curve([[x, y * 13, z], [x + 4, y * (24 + k * 3), z - 12], [x + 6 + k * 5, y * (28 + k * 3), 3]]), 4, 2.2, k % 2 ? orange : red);
    }
    // head: carapace plate, rostrum, mouthparts
    S(a, "sphere", [14, 0, 50], [36, 32, 24], teal, null, GLOSS);
    S(a, "sphere", [10, 0, 56], [28, 26, 11], tealL, null, GLOSS);
    S(a, "cube", [10, 0, 62], [22, 1.5, 1.6], [0.02, 0.2, 0.22]);
    for (const y of [-1, 1]) for (let k = 0; k < 3; k++) S(a, "sphere", [2 + k * 8, y * 8, 60 - k], [3, 3, 1.5], gold);
    S(a, "cone", [31, 0, 53], [10, 8, 14], tealL, { dir: [1, 0, 0.2] }, GLOSS);
    S(a, "sphere", [28, 0, 43], [12, 18, 8], red);
    // antennal scales and long feelers
    for (const y of [-1, 1]) {
      S(a, "sphere", [36, y * 13, 50], [22, 11, 2.5], orange, [-20, y * -22, 0], GLOSS);
      S(a, "sphere", [44, y * 16, 53], [7, 7, 3], blue, null, GLOSS);
      polyline(a, curve([[30, y * 5, 56], [48, y * 12, 72], [58, y * 24, 84], [56, y * 34, 88]]), 7, 1.5, orange);
    }
    // stalked eyes with the mantis shrimp's dark midband
    const eyes = [];
    for (const y of [-1, 1]) {
      const st = P(a, [24, y * 7, 58], "Tentacle");
      S(st, "cylinder", [0, 0, 5], [4.5, 4.5, 12], orange);
      S(st, "sphere", [1, 0, 16], [13, 10, 16], [0.25, 0.85, 0.75], null, GLOSS);
      S(st, "sphere", [1, 0, 16], [13.8, 10.8, 3.2], [0.06, 0.15, 0.35]);
      S(st, "sphere", [6.5, 0, 16], [3, 4.5, 6], C.black, null, GLOSS);
      S(st, "sphere", [7.5, y * 1.5, 19], [1.8, 1.8, 1.8], C.white, null, { emissive: [0.7, 0.7, 0.7] });
      eyes.push(st);
    }
    // the raptorial clubs: folded under the head, whip forward on a punch
    const clubs = [];
    for (const y of [-1, 1]) {
      const sh = P(a, [24, y * 13, 42]);
      tube(sh, [0, 0, 0], [12, y * 3, -12], 8, [0.9, 0.25, 0.45], GLOSS);
      S(sh, "sphere", [12, y * 3, -12], [10, 10, 10], blue, null, GLOSS);
      for (let k = 0; k < 4; k++) S(sh, "sphere", [3 + k * 2.7, y * (3.5 + k * 0.7), -3 - k * 2.7], [2.2, 2.2, 2.2], gold);
      const fore = P(sh, [12, y * 3, -12], "Club");
      tube(fore, [0, 0, 0], [16, 0, 0], 7, [0.95, 0.35, 0.3], GLOSS);
      S(fore, "sphere", [25, 0, 0], [22, 17, 17], red, null, GLOSS);
      S(fore, "sphere", [32, 0, 0], [10, 13, 13], [1, 0.75, 0.25], null, GLOSS);
      for (let k = 0; k < 12; k++) { const d = fib(k, 12); if (d[0] < 0.1) continue; S(fore, "sphere", [25 + d[0] * 10.5, d[1] * 8.4, d[2] * 8.4], [2.6, 2.6, 2.6], C.black); }
      S(fore, "cone", [20, 0, -8], [5, 5, 10], cream, { dir: [0.2, 0, -1] });
      clubs.push(fore);
    }
    // cavitation flash in front of the club: only drawn on a punch
    const flashM = mat([0.85, 0.97, 1], { emissive: [0.6, 0.9, 1], opacity: 0, unique: true });
    const flash = fx(S(a, "sphere", [64, 0, 30], [24, 24, 24], 0, null, { material: flashM }));
    const bubbleM = mat([0.85, 0.97, 1], { emissive: [0.2, 0.3, 0.35], opacity: 0, unique: true });
    const bubbles = []; for (let i = 0; i < 8; i++) bubbles.push(fx(S(a, "sphere", [64, 0, 30], [5, 5, 5], 0, null, { material: bubbleM })));
    return (t) => {
      sweep(a, t, 0.3);
      const ph = (t % 2) / 2;
      const whip = ph < 0.05 ? ph / 0.05 : ph < 0.14 ? 1 : ph < 0.5 ? 1 - (ph - 0.14) / 0.36 : 0;
      clubs.forEach(c => { c.rotation.y = (-118 + whip * 128) * D2R; });
      a.position.x = whip * 4;
      eyes.forEach((e, i) => { e.rotation.x = Math.sin(t * 1.9 + i * 2.4) * 0.3; e.rotation.z = Math.sin(t * 1.3 + i * 4) * 0.5; });
      tail.rotation.z = Math.sin(t * 2.2) * 0.12;
      const f = ph > 0.03 && ph < 0.3 ? (ph - 0.03) / 0.27 : 0;
      flashM.opacity = f ? 0.7 * (1 - f) : 0; flash.visible = !!f; flash.scale.setScalar(0.1 + f * 0.7);
      bubbleM.opacity = f ? 0.8 * (1 - f) : 0;
      bubbles.forEach((b, i) => { const d = fib(i, 8); b.visible = !!f; b.position.set(64 + d[0] * f * 26, d[1] * f * 26, 30 + d[2] * f * 22 + f * 8); });
    };
  },

  // ------------------------------------------------------------------ Toxin Shooter
  lionfish(root) {
    const maroon = [0.55, 0.07, 0.06], rust = [0.82, 0.3, 0.12], cream = [0.98, 0.9, 0.76], venom = mat([0.45, 1, 0.3], { emissive: [0.25, 0.85, 0.12] });
    // poisoned coral head: stony dome, pale polyps, green-tipped sponges
    S(root, "sphere", [0, 0, 8], [74, 74, 22], [0.3, 0.28, 0.3], null, MATTE);
    rockRing(root, 9, 32, 7, 0.6, 15);
    S(root, "sphere", [0, 0, 16], [54, 54, 22], [0.58, 0.32, 0.52]);
    for (let g = 0; g < 4; g++) S(root, "sphere", [0, 0, 18 + g * 2.4], [54 - g * 10, 54 - g * 10, 8], g % 2 ? [0.7, 0.42, 0.62] : [0.45, 0.22, 0.42]);
    around(12, 20, 21, (x, y, z, an, i) => { S(root, "cylinder", [x, y, z + 2], [3, 3, 6], [0.9, 0.7, 0.8]); S(root, "sphere", [x, y, z + 6], [5, 5, 5], i % 2 ? [0.95, 0.8, 0.9] : [0.9, 1, 0.75]); });
    for (let i = 0; i < 4; i++) {
      const an = i * 1.57 + 0.5, bx = Math.cos(an) * 34, by = Math.sin(an) * 34;
      for (let k = 0; k < 3; k++) { const x = bx + (k - 1) * 4, h = 12 + k * 4; S(root, "cylinder", [x, by + (k % 2) * 3, 16 + h / 2], [6, 6, h], [0.5, 0.2, 0.55]); S(root, "cylinder", [x, by + (k % 2) * 3, 16 + h], [3.8, 3.8, 1.2], 0, null, { material: venom }); }
    }

    const a = P(root, [0, 0, 66], "Anim"); P(a, [36, 0, 4], "Muzzle");
    // body and banded stripes
    S(a, "sphere", [-2, 0, 0], [58, 22, 34], cream);
    S(a, "sphere", [-30, 0, 1], [20, 9, 15], cream);
    S(a, "sphere", [2, 0, -9], [46, 18, 15], [1, 0.95, 0.86]);
    for (let x = -26; x <= 24; x += 5.2) {
      const k = Math.sqrt(Math.max(0, 1 - ((x + 2) / 29.5) ** 2)), wide = Math.round((x + 26) / 5.2) % 3 === 0;
      S(a, "sphere", [x, 0, 0], [wide ? 3.8 : 2.2, 22.8 * k + 0.4, 34.8 * k + 0.4], wide ? maroon : rust, [(x * 0.3), 0, 0]);
    }
    // head with lumpy brow, jowls and a pout
    S(a, "sphere", [17, 0, 3], [26, 20, 26], cream);
    for (let k = 0; k < 4; k++) S(a, "sphere", [14 + k * 4.5, 0, 3], [2.2, 20.4 - k * 2.2, 26 - k * 3], maroon, [-22, 0, 0]);
    for (const y of [-1, 1]) {
      S(a, "sphere", [21, y * 9.2, 10], [10, 6, 10], [1, 0.55, 0.15], null, GLOSS);
      S(a, "sphere", [24, y * 11.4, 10], [5, 2.5, 6], C.black, null, GLOSS);
      S(a, "sphere", [25.4, y * 11.8, 12], [1.6, 1.6, 1.6], C.white, null, { emissive: [0.7, 0.7, 0.7] });
      for (let k = 0; k < 3; k++) { tube(a, [19 + k * 2, y * (8 - k), 15], [18 + k * 3, y * (10 + k), 24 + (k % 2) * 3], 1.4, maroon); S(a, "sphere", [18 + k * 3, y * (10 + k), 24 + (k % 2) * 3], [2.8, 2.8, 2.8], rust); }
      polyline(a, curve([[29, y * 5, -3], [33, y * 9, -8], [31, y * 12, -14]]), 3, 1.4, rust);
    }
    S(a, "sphere", [30, 0, -1], [9, 12, 5], [0.98, 0.72, 0.55], null, GLOSS);
    S(a, "sphere", [31.5, 0, -2.5], [6, 9, 2], [0.3, 0.06, 0.06]);
    // venom-tipped dorsal crown: tall banded spines over a sheer web
    S(a, "sphere", [-4, 0, 26], [44, 1.6, 24], maroon, [8, 0, 0], { opacity: 0.45 });
    S(a, "sphere", [-27, 0, 20], [22, 1.6, 20], rust, [-14, 0, 0], { opacity: 0.45 });
    const spineTips = [];
    for (let i = 0; i < 13; i++) {
      const u = i / 12, x = 16 - u * 38, top = 16 * Math.sqrt(Math.max(0, 1 - ((x + 2) / 30) ** 2)), L = 44 - Math.abs(u - 0.3) * 34;
      const dir = [-0.28 - u * 0.25, 0, 1], b = [x, 0, top], e = [x + dir[0] * L, 0, top + L];
      for (let k = 0; k < 5; k++) { const p0 = k / 5, p1 = (k + 1) / 5; tube(a, [b[0] + (e[0] - b[0]) * p0, 0, b[2] + (e[2] - b[2]) * p0], [b[0] + (e[0] - b[0]) * p1, 0, b[2] + (e[2] - b[2]) * p1], 2.6 - k * 0.3, k % 2 ? cream : maroon); }
      S(a, "cone", [e[0] + dir[0] * 3, 0, e[2] + 3], [2.6, 2.6, 7], 0, dir, { material: venom }).quaternion.setFromUnitVectors(Z, new THREE.Vector3(...dir).normalize());
    }
    // pectoral fans: rays and membrane share one flat plane, so the fan can flare as a unit
    const fans = [];
    for (const y of [-1, 1]) {
      const fan = P(a, [12, y * 9, 0], "Tentacle");
      S(fan, "sphere", [-21, 0, 0], [46, 1.2, 66], cream, null, { opacity: 0.55 });
      S(fan, "sphere", [-18, 0, 0], [26, 1.3, 40], rust, null, { opacity: 0.5 });
      for (let k = 0; k < 11; k++) {
        const an = (k / 10 - 0.5) * 2.5, L = 39 - Math.abs(k / 10 - 0.5) * 12, d = [-Math.cos(an), 0, Math.sin(an)];
        for (let s = 0; s < 6; s++) tube(fan, [d[0] * L * s / 6, 0, d[2] * L * s / 6], [d[0] * L * (s + 1) / 6, 0, d[2] * L * (s + 1) / 6], 1.9 - s * 0.15, s % 2 ? cream : maroon);
        S(fan, "cone", [d[0] * (L + 2), 0, d[2] * (L + 2)], [1.8, 1.8, 5], 0, null, { material: venom }).quaternion.setFromUnitVectors(Z, new THREE.Vector3(...d));
      }
      fans.push({ fan, y });
      S(a, "sphere", [2, y * 8, -13], [22, 3, 5], maroon, [-20, y * 12, 0]);
    }
    S(a, "sphere", [-14, 0, -19], [18, 1.6, 14], rust, [20, 0, 0], { opacity: 0.6 });
    for (let k = 0; k < 3; k++) { tube(a, [-8 - k * 4, 0, -14], [-16 - k * 4, 0, -30], 1.6, k % 2 ? cream : maroon); S(a, "cone", [-16 - k * 4, 0, -32], [1.8, 1.8, 4], 0, { dir: [0, 0, -1] }, { material: venom }); }
    // spotted tail fan
    const tail = P(a, [-38, 0, 1], "Tail");
    S(tail, "sphere", [-10, 0, 0], [24, 1.4, 30], cream, null, { opacity: 0.8 });
    for (let k = 0; k < 7; k++) { const an = (k / 6 - 0.5) * 1.3, dx = -Math.cos(an) * 21, dz = Math.sin(an) * 21; tube(tail, [0, 0, 0], [dx, 0, dz], 1, maroon); for (let s = 1; s < 4; s++) S(tail, "sphere", [dx * s / 3.4, 0, dz * s / 3.4], [2.6, 2.2, 2.6], maroon); }
    // the shot: a venom spine with a glowing tip
    const shot = fx(P(a, [36, 0, 4]));
    S(shot, "cylinder", [0, 0, 0], [2.4, 2.4, 22], C.bone, { dir: [1, 0, 0] });
    S(shot, "cone", [14, 0, 0], [5, 5, 9], 0, { dir: [1, 0, 0] }, { material: venom });
    S(shot, "sphere", [-5, 0, 0], [5, 5, 5], maroon);
    const drops = []; for (let i = 0; i < 5; i++) drops.push(fx(S(a, "sphere", [0, 0, 0], [3, 3, 4], 0, null, { material: venom })));
    return (t) => {
      sweep(a, t, 0.35);
      const ph = (t % 2.2) / 2.2, kick = ph < 0.12 ? 1 - ph / 0.12 : 0;
      a.position.z = 66 + Math.sin(t * 1.5) * 3.5; a.rotation.y = -kick * 0.12;
      fans.forEach(({ fan, y }) => { fan.rotation.x = y * (-14 + Math.sin(t * 1.5) * 5) * D2R; fan.rotation.z = y * (-58 - kick * 16 + Math.sin(t * 1.5 + 1) * 5) * D2R; });
      tail.rotation.z = Math.sin(t * 3) * 0.3;
      shot.position.x = 34 + (ph < 0.45 ? ph / 0.45 * 90 : 0); shot.visible = ph < 0.45 || ph > 0.7;
      if (ph >= 0.45) shot.position.x = 34;
      drops.forEach((d, i) => { const u = (t * 0.35 + i / 5) % 1; d.position.set(-14 + i * 7, (i % 2 ? 1 : -1) * 4, -18 - u * 28); d.visible = u < 0.85; });
    };
  },

  // ------------------------------------------------------------------ Razor Blade Shooter
  sawfish(root) {
    const top = [0.3, 0.29, 0.2], topD = [0.2, 0.19, 0.13], belly = [0.92, 0.88, 0.74], tooth = [0.97, 0.94, 0.82], blood = [0.75, 0.05, 0.05], snout = [0.58, 0.52, 0.34], sand = [0.72, 0.62, 0.42];
    // scraped sand bed with sea grass, ribs and loose teeth
    S(root, "sphere", [0, 0, 5], [88, 88, 18], sand, null, MATTE);
    S(root, "sphere", [-4, 0, 10], [62, 50, 18], [0.78, 0.68, 0.48], null, MATTE);
    const R = rng(21);
    for (let i = 0; i < 12; i++) { const an = i / 12 * 6.28, r = 38; S(root, "sphere", [Math.cos(an) * r, Math.sin(an) * r, 4], [14, 12, 6], i % 2 ? [0.6, 0.52, 0.34] : [0.68, 0.58, 0.38], null, MATTE); }
    for (const [cx, cy, n] of [[-30, 28, 5], [24, -32, 4], [-34, -22, 3]]) for (let k = 0; k < n; k++) { const h = 14 + R() * 18, x = cx + (k - n / 2) * 3.2, y = cy + R() * 5, d = [(R() - 0.5) * 0.5, (R() - 0.5) * 0.5, 1]; S(root, "cone", [x + d[0] * h / 2, y + d[1] * h / 2, 8 + h / 2], [3.5, 1.6, h], [0.25, 0.6, 0.3], { dir: d }); }
    for (let k = 0; k < 5; k++) { const x = 18 + k * 5; tube(root, [x, 18, 7], [x + 2, 28, 16], 2.4, C.bone); tube(root, [x, 18, 7], [x + 2, 8, 15], 2.4, C.bone); }
    tube(root, [16, 18, 8], [44, 18, 8], 3.4, C.bone);
    for (let i = 0; i < 7; i++) { const an = R() * 6.28, r = 18 + R() * 22; S(root, "cone", [Math.cos(an) * r, Math.sin(an) * r, 12], [3, 1.4, 8], tooth, { dir: [Math.cos(i), Math.sin(i), 0.2] }); }

    const a = P(root, [0, 0, 16], "Anim"); P(a, [60, 0, 10], "Muzzle");
    const spine = curve([[-48, 0, 26], [-36, 0, 19], [-22, 0, 14], [-6, 0, 12], [10, 0, 11], [20, 0, 10]]);
    const prof = [7, 11, 18, 27, 37, 40, 36], N = 14;
    for (let i = 0; i < N; i++) {
      const u = i / (N - 1), f = u * (prof.length - 1), j = Math.min(prof.length - 2, Math.floor(f)), w = prof[j] + (prof[j + 1] - prof[j]) * (f - j), p = spine(u);
      S(a, "sphere", p, [24, w, w * 0.56], top);
      S(a, "sphere", [p[0], 0, p[2] - w * 0.13], [24, w * 0.92, w * 0.34], belly);
    }
    // mottling and the gill slits
    for (let i = 0; i < 20; i++) { const u = 0.22 + R() * 0.68, p = spine(u), y = (R() - 0.5) * 22; S(a, "sphere", [p[0], y, p[2] + 9 - Math.abs(y) * 0.28], [5, 4, 1.5], topD, { dir: [0, y * 0.06, 1] }); }
    for (const y of [-1, 1]) for (let k = 0; k < 5; k++) S(a, "cube", [13 - k * 3, y * 16, 6], [1, 7, 1.2], [0.2, 0.15, 0.1], [0, y * 18, 0]);
    // flat head, eyes on top, spiracles
    S(a, "sphere", [20, 0, 11], [32, 32, 12], top);
    S(a, "sphere", [21, 0, 8.5], [30, 30, 7], belly);
    for (const y of [-1, 1]) {
      S(a, "sphere", [17, y * 9, 16], [8, 7, 6], [0.25, 0.22, 0.14]);
      S(a, "sphere", [18, y * 10.5, 17.6], [4.5, 3.6, 3.6], [0.8, 0.65, 0.2], null, GLOSS);
      S(a, "sphere", [19, y * 11.4, 18], [2.5, 1.8, 2.5], C.black, null, GLOSS);
      S(a, "sphere", [10, y * 7, 16.5], [4, 3, 1.5], [0.1, 0.08, 0.06]);
    }
    // the saw: a tapering blade with ivory teeth, the outer ones bloodied
    const blade = P(a, [32, 0, 10], "Blade");
    const BL = 38;
    for (let k = 0; k < 8; k++) { const u0 = k / 8, x = u0 * BL + BL / 16, w = 11.5 - u0 * 5; S(blade, "cube", [x, 0, 0], [BL / 8 + 0.6, w, 3], k % 2 ? snout : [0.52, 0.47, 0.3]); }
    S(blade, "cylinder", [BL + 0.5, 0, 0], [6.6, 6.6, 3], snout);
    S(blade, "cube", [BL / 2, 0, 1.7], [BL, 1.6, 1], [0.38, 0.33, 0.2]);
    for (let k = 0; k < 6; k++) S(blade, "sphere", [4 + k * 6, 2.5, 1.8], [2, 1.2, 0.8], [0.3, 0.26, 0.15]);
    for (const y of [-1, 1]) for (let k = 0; k < 12; k++) {
      const u = k / 11, x = 1 + u * (BL - 1), wid = 5.75 - u * 2.5, L = 9.5 - u * 2;
      S(blade, "cone", [x, y * (wid + L / 2 - 0.5), 0], [4, 2, L], tooth, { dir: [0.14, y, 0] }, GLOSS);
      if (k > 6) S(blade, "cone", [x + 0.6, y * (wid + L - 0.5), 0], [1.8, 1.1, 3], blood, { dir: [0.14, y, 0] });
    }
    // broad pectorals, thin dorsals, shark tail
    const fins = [];
    for (const y of [-1, 1]) {
      const f = P(a, [6, y * 15, 7], "Tentacle");
      S(f, "sphere", [-8, y * 13, -2], [28, 28, 2.6], top, [0, y * 22, y * 10]);
      S(f, "sphere", [-8, y * 13, -3], [26, 26, 2], belly, [0, y * 22, y * 10]);
      fins.push(f);
      S(a, "sphere", [-22, y * 11, 11], [15, 13, 2], top, [0, y * 15, y * 10]);
    }
    S(a, "cone", [-7, 0, 26], [20, 2.8, 22], topD, { dir: [-0.5, 0, 1] });
    S(a, "cone", [-31, 0, 25], [13, 2.4, 15], topD, { dir: [-0.6, 0, 1] });
    const tail = P(a, [-48, 0, 26], "Tail");
    S(tail, "cone", [-10, 0, 13], [16, 2.6, 33], topD, { dir: [-0.65, 0, 1] });
    S(tail, "cone", [-5, 0, -5], [11, 2.4, 15], topD, { dir: [-0.8, 0, -0.7] });
    // the shot: a thrown tooth that spins
    const shot = fx(P(a, [60, 0, 10]));
    const spin = P(shot, [0, 0, 0]);
    for (let k = 0; k < 3; k++) { const an = k / 3 * Math.PI * 2; S(spin, "cone", [Math.cos(an) * 6, Math.sin(an) * 6, 0], [5, 2.4, 13], tooth, { dir: [Math.cos(an), Math.sin(an), 0] }, GLOSS); }
    S(spin, "cylinder", [0, 0, 0], [6, 6, 2.5], blood);
    const bloodM = mat(blood, { emissive: [0.4, 0, 0], opacity: 0.75, unique: true });
    const wisps = []; for (let i = 0; i < 6; i++) wisps.push(fx(S(a, "sphere", [0, 0, 0], [5, 5, 4], 0, null, { material: bloodM })));
    return (t) => {
      const ph = (t % 2.4) / 2.4;
      const slash = ph < 0.18 ? Math.sin(ph / 0.18 * Math.PI * 2) : 0;
      a.rotation.z = Math.sin(t * 0.55) * 0.35 + slash * 0.35;
      a.position.z = 16 + Math.sin(t * 1.3) * 1.5;
      tail.rotation.z = Math.sin(t * 2.6) * 0.3;
      fins.forEach((f, i) => { f.rotation.x = (i ? 1 : -1) * Math.sin(t * 1.3) * 0.12; });
      const fly = ph > 0.06 && ph < 0.5 ? (ph - 0.06) / 0.44 : 0;
      shot.visible = !!fly; shot.position.x = 60 + fly * 75; spin.rotation.z = t * 16;
      wisps.forEach((w, i) => { w.visible = !!fly; w.position.set(60 + (fly - i * 0.05) * 70, Math.sin(i * 2) * 2 * i * fly, 10 - i * fly); w.scale.setScalar(Math.max(0.01, 0.05 * (1 - i / 7))); });
    };
  },

  // ------------------------------------------------------------------ Motivator
  humpback(root) {
    const slate = [0.09, 0.13, 0.21], slateL = [0.15, 0.2, 0.3], pale = [0.9, 0.93, 0.95], groove = [0.3, 0.34, 0.42], yellow = [1, 0.8, 0.15], blueF = [0.1, 0.5, 0.95];
    // coral knoll the whale sings over
    S(root, "sphere", [0, 0, 7], [78, 78, 20], [0.68, 0.42, 0.38], null, MATTE);
    rockRing(root, 9, 32, 7, 0.1, 14);
    S(root, "cylinder", [0, 0, 14], [44, 44, 10], [0.9, 0.55, 0.45]);
    S(root, "cylinder", [0, 0, 20], [30, 30, 6], [0.97, 0.68, 0.55]);
    around(14, 22, 16, (x, y, z, an, i) => S(root, "sphere", [x, y, z + 2], [8, 8, 7], i % 2 ? [1, 0.75, 0.6] : [0.95, 0.5, 0.5]));
    for (const [an, c] of [[0.6, [1, 0.5, 0.25]], [2.7, [0.85, 0.3, 0.7]], [4.6, [0.3, 0.85, 0.65]]]) {
      const bx = Math.cos(an) * 33, by = Math.sin(an) * 33;
      for (let k = 0; k < 5; k++) { const b = (k / 5) * 6.28, tip = [bx + Math.cos(b) * 8, by + Math.sin(b) * 8, 28 + (k % 2) * 6]; tube(root, [bx, by, 12], tip, 3.2, c); S(root, "sphere", tip, [5, 5, 5], c.map(v => Math.min(1, v + 0.2))); }
    }
    // giant clam on the knoll
    S(root, "sphere", [-20, 28, 22], [20, 14, 7], [0.25, 0.4, 0.7]); S(root, "sphere", [-20, 28, 25], [20, 14, 6], [0.5, 0.25, 0.7], [-16, 0, 0]);

    const a = P(root, [0, 0, 0], "Anim");
    const spine = curve([[-40, 0, 84], [-32, 0, 72], [-18, 0, 62], [0, 0, 60], [18, 0, 66], [30, 0, 78]]);
    const N = 16, width = (u) => 9 + 41 * Math.pow(Math.sin(Math.PI * Math.min(1, u * 0.86 + 0.02)), 0.85) * (0.55 + u * 0.45);
    const normal = (u) => { const p = spine(Math.max(0, u - 0.03)), q = spine(Math.min(1, u + 0.03)), L = Math.hypot(q[0] - p[0], q[2] - p[2]); return [(q[2] - p[2]) / L, 0, -(q[0] - p[0]) / L]; };
    for (let i = 0; i < N; i++) {
      const u = i / (N - 1), p = spine(u), w = width(u), n = normal(u);
      S(a, "sphere", p, [w * 0.8 + 14, w * 0.92, w], slate);
      if (u > 0.42) S(a, "sphere", [p[0] + n[0] * w * 0.24, 0, p[2] + n[2] * w * 0.24], [w * 0.7 + 12, w * 0.78, w * 0.68], pale);
    }
    // throat grooves run along the white belly
    for (let k = -4; k <= 4; k++) {
      const ph = k * 0.3;
      polyline(a, (v) => { const u = 0.48 + v * 0.52, p = spine(u), w = width(u) * 0.98, n = normal(u), rad = w * 0.56; return [p[0] + n[0] * rad * Math.cos(ph), Math.sin(ph) * w * 0.47, p[2] + n[2] * rad * Math.cos(ph)]; }, 8, 1.7, groove);
    }
    // head: upper jaw, mouth line, knobs, eyes, blowholes
    S(a, "sphere", [33, 0, 84], [34, 27, 20], slate);
    S(a, "sphere", [36, 0, 79], [31, 27, 15], pale);
    for (const y of [-1, 1]) {
      polyline(a, curve([[48, 0, 79], [36, y * 14, 78], [25, y * 19, 76], [19, y * 21, 71]]), 7, 1.8, C.black);
      S(a, "sphere", [10, y * 20.5, 68], [6, 3, 6], pale);
      S(a, "sphere", [11, y * 21.7, 68.5], [3.2, 2, 3.2], C.black, null, GLOSS);
      S(a, "sphere", [12, y * 22.3, 69.4], [1.3, 1.3, 1.3], C.white, null, { emissive: [0.7, 0.7, 0.7] });
      for (let k = 0; k < 5; k++) S(a, "sphere", [44 - k * 6.5, y * (5 + k * 1.6), 92 - k * 0.2], [3.4, 3.4, 2.8], slateL);
    }
    S(a, "sphere", [20, -2.5, 92], [3.5, 2, 1.5], C.black); S(a, "sphere", [20, 2.5, 92], [3.5, 2, 1.5], C.black);
    const R = rng(77);
    for (let i = 0; i < 10; i++) { const u = 0.7 + R() * 0.3, p = spine(u), w = width(u) * 0.5, y = (R() - 0.5) * 1.5 * w, n = normal(u); barnacle(a, [p[0] + n[0] * Math.sqrt(Math.max(1, w * w - y * y)), y, p[2] + n[2] * Math.sqrt(Math.max(1, w * w - y * y))], 3.6 + R() * 2, [n[0], y / w, n[2]]); }
    for (let k = 0; k < 8; k++) { const y = (k % 2 ? 1 : -1) * (6 + k), z = 73 - k * 0.8; barnacle(a, [46 - k * 3, y, z], 4, [0.5, y * 0.05, -0.8]); }
    // small dorsal nub with the hump
    const d0 = spine(0.3), nd = normal(0.3);
    S(a, "cone", [d0[0] - nd[0] * 13, 0, d0[2] - nd[2] * 13 + 2], [15, 4, 13], slate, { dir: [-0.5, 0, 1] });
    // long flippers with a white underside and knobbed leading edge
    const fins = [];
    for (const y of [-1, 1]) {
      const f = P(a, [14, y * 20, 60], "Tentacle");
      const fin = curve([[0, 0, 0], [-1, y * 11, -6], [-6, y * 22, -13], [-13, y * 33, -19], [-20, y * 41, -23]]);
      for (let k = 0; k < 16; k++) {
        const u = k / 15, p = fin(u), wid = 15 * Math.sin(Math.PI * (0.2 + u * 0.72)) + 1;
        S(f, "sphere", p, [wid, 13, 3.2], slate, [-14, y * 28, y * 24]);
        S(f, "sphere", [p[0], p[1], p[2] - 1.1], [wid * 0.9, 12, 2.4], pale, [-14, y * 28, y * 24]);
        if (k % 3 === 0) S(f, "sphere", [p[0] + wid * 0.48, p[1], p[2] + 0.2], [3.2, 3.2, 2.6], slateL);
      }
      fins.push({ f, y });
    }
    // fluke with a notch and a scalloped trailing edge
    const tail = P(a, [-40, 0, 84], "Tail");
    tail.rotation.y = -0.6;
    for (const y of [-1, 1]) {
      S(tail, "sphere", [-8, y * 14, 2], [20, 31, 3], slate, [0, y * -22, 0]);
      S(tail, "sphere", [-8, y * 14, 0.6], [17, 26, 1.8], pale, [0, y * -22, 0]);
      for (let k = 0; k < 6; k++) { const v = k / 5; S(tail, "sphere", [-17 + v * 6 - (1 - v) * 1, y * (3 + v * 25), 2], [4.6, 5, 2.2], slate); }
    }
    S(tail, "sphere", [2, 0, 0], [12, 9, 8], slate);
    // cleaner fish orbit in place of the old music notes
    const spin = P(root, [0, 0, 0], "Spinner"), fish = [];
    for (let i = 0; i < 7; i++) {
      const g = P(spin, [0, 0, 0]), c = i % 2 ? yellow : blueF;
      S(g, "sphere", [0, 0, 0], [13, 5, 7], c); S(g, "sphere", [2, 0, -1.2], [8, 4.2, 4], C.white);
      S(g, "cube", [0, 0, 0], [10, 5.4, 1.2], C.black);
      S(g, "cone", [-9, 0, 0], [6, 1.6, 8], c, { dir: [1, 0, 0] });
      S(g, "sphere", [4.6, 2.2, 1], [1.8, 1.2, 1.8], C.black); S(g, "sphere", [4.6, -2.2, 1], [1.8, 1.2, 1.8], C.black);
      fish.push(g);
    }
    // song rings: expanding bubble circles that show the aura pulse
    const ringM = mat([0.85, 0.97, 1], { emissive: [0.25, 0.4, 0.45], opacity: 0, unique: true });
    const ring = fx(pulseRing(root, 30, 40, 6, 5, ringM));
    const songM = mat([1, 0.95, 0.7], { emissive: [0.4, 0.3, 0.1], opacity: 0, unique: true });
    const song = fx(pulseRing(root, 22, 26, 84, 4, songM));
    return (t) => {
      a.position.z = Math.sin(t * 1.4) * 3; a.rotation.z = t * 25 * D2R;
      const ph = (t % 3) / 3, beat = Math.pow(Math.max(0, Math.sin(t * Math.PI / 1.5)), 12);
      a.scale.setScalar(1 + 0.04 * beat);
      fins.forEach(({ f, y }) => { f.rotation.x = y * Math.sin(t * 1.4) * 0.26; });
      tail.rotation.y = -0.6 + Math.sin(t * 1.4) * 0.22;
      spin.rotation.z = -t * 40 * D2R;
      fish.forEach((g, i) => { const an = i / 7 * 6.283 + Math.sin(i * 3) * 0.3; g.position.set(Math.cos(an) * (48 + (i % 3) * 4), Math.sin(an) * (48 + (i % 3) * 4), 32 + (i % 4) * 11 + Math.sin(t * 2 + i) * 3); g.rotation.z = an - Math.PI / 2 + Math.PI; });
      ring.scale.set(0.5 + ph * 1.1, 0.5 + ph * 1.1, 1); ringM.opacity = 0.85 * (1 - ph);
      const p2 = (ph + 0.5) % 1; song.scale.set(0.3 + p2 * 1.5, 0.3 + p2 * 1.5, 1); songM.opacity = 0.65 * (1 - p2); song.position.z = 112 + Math.sin(t * 1.4) * 3;
    };
  },

  // ------------------------------------------------------------------ Instigator
  vent(root) {
    const char = [0.09, 0.08, 0.09], ash = [0.2, 0.18, 0.18], rust = [0.5, 0.24, 0.1], ochre = [0.72, 0.5, 0.14], sulfur = [0.95, 0.82, 0.2], worm = [0.95, 0.92, 0.84], plume = [0.95, 0.1, 0.12];
    const hot = mat([1, 0.45, 0.08], { emissive: [1, 0.35, 0.03] }), acid = mat([0.8, 1, 0.2], { emissive: [0.55, 0.8, 0.05] });
    const R = rng(31);
    // basalt shelf with hot fissures, sulfur crust and a white microbial mat
    S(root, "sphere", [0, 0, 5], [84, 84, 16], [0.12, 0.11, 0.12], null, MATTE);
    for (let i = 0; i < 11; i++) { const an = i / 11 * 6.283, r = 33, s = 14 + (i % 3) * 5; S(root, "sphere", [Math.cos(an) * r, Math.sin(an) * r, 8], [s + 5, s, s * 0.75], i % 2 ? char : ash, [0, an / D2R, 0], MATTE); }
    for (let i = 0; i < 6; i++) { const an = i * 1.05 + 0.3; S(root, "cube", [Math.cos(an) * 28, Math.sin(an) * 28, 11.5], [22, 1.8, 1.2], 0, [0, an / D2R + 25, 0], { material: hot }); }
    for (let i = 0; i < 10; i++) { const an = R() * 6.283, r = 20 + R() * 20; S(root, "sphere", [Math.cos(an) * r, Math.sin(an) * r, 12 + R() * 3], [9 + R() * 7, 8 + R() * 5, 2.5], i % 3 ? sulfur : C.white, null, MATTE); }
    // main chimney: lumpy mineral stack that narrows and rusts toward the top
    function chimney(x, y, H, W, n) {
      for (let k = 0; k < n; k++) {
        const u = k / (n - 1), z = 10 + u * H, w = W * (1 - u * 0.58), c = [char, ash, rust, char, ochre, ash][((k * 5 + Math.round(x)) % 6 + 6) % 6];
        S(root, "cone", [x, y, z + 3], [w * 1.15, w * 1.1, H / n * 1.9], c, [(k % 3 - 1) * 4, k * 37, (k % 2) * 5], MATTE);
        for (let m = 0; m < 5; m++) { const an = R() * 6.283, s = w * (0.25 + R() * 0.22); S(root, "sphere", [x + Math.cos(an) * w * 0.42, y + Math.sin(an) * w * 0.42, z + (R() - 0.5) * 7], [s, s, s * 0.8], m === 0 && k % 3 === 1 ? sulfur : m === 1 ? rust : c, null, MATTE); }
        if (k % 2) { const an = R() * 6.283; S(root, "cube", [x + Math.cos(an) * w * 0.45, y + Math.sin(an) * w * 0.45, z], [1.6, 1.6, H / n * 1.2], 0, [(R() - 0.5) * 30, 0, (R() - 0.5) * 30], { material: hot }); }
      }
      const top = 10 + H + 5, tw = W * 0.42;
      S(root, "cylinder", [x, y, top], [tw * 1.15, tw * 1.15, 6], ash, null, MATTE);
      S(root, "cylinder", [x, y, top + 3.2], [tw * 0.66, tw * 0.66, 1], 0, null, { material: hot });
      around(7, tw * 0.56, top + 3, (ax, ay) => S(root, "sphere", [x + ax, y + ay, top + 3], [4.5, 4.5, 3.5], sulfur, null, MATTE));
      return top + 4;
    }
    const mainTop = chimney(-2, 0, 66, 44, 9);
    const sideA = chimney(24, -17, 26, 22, 5), sideB = chimney(-23, 22, 16, 18, 4);
    // tube worms crowd the warm rock, plumes swaying
    const plumes = [];
    for (const [cx, cy, n] of [[20, 24, 6], [-27, -20, 6], [31, 5, 3]]) for (let i = 0; i < n; i++) {
      const bx = cx + (R() - 0.5) * 14, by = cy + (R() - 0.5) * 14, h = 22 + R() * 22, lean = [(R() - 0.5) * 8, (R() - 0.5) * 8];
      tube(root, [bx, by, 10], [bx + lean[0], by + lean[1], 10 + h], 4.6, worm, MATTE);
      for (let k = 0; k < 4; k++) S(root, "cylinder", [bx + lean[0] * k / 4, by + lean[1] * k / 4, 12 + h * k / 4], [5.4, 5.4, 1], [0.8, 0.76, 0.66]);
      const g = P(root, [bx + lean[0], by + lean[1], 10 + h], "Tentacle");
      S(g, "cone", [0, 0, 4], [5, 5, 8], plume, { dir: [0, 0, -1] });
      around(9, 3.4, 8, (x, y, z, an) => S(g, "cone", [x, y, z + 2], [3, 3, 10], plume, { dir: [Math.cos(an) * 0.55, Math.sin(an) * 0.55, 1] }));
      S(g, "sphere", [0, 0, 8], [6, 6, 4], [1, 0.3, 0.25]);
      plumes.push(g);
    }
    // a pale vent crab on the rim
    const crab = P(root, [32, -28, 15]);
    S(crab, "sphere", [0, 0, 0], [12, 15, 7], worm);
    for (const y of [-1, 1]) { tube(crab, [2, y * 5, 0], [8, y * 13, -3], 1.8, worm); S(crab, "sphere", [9, y * 13, -3], [4, 3, 3], [0.92, 0.75, 0.68]); for (let k = 0; k < 3; k++) tube(crab, [-3 + k * 2.5, y * 6, 0], [-5 + k * 3, y * 13, -6], 1.3, worm); }
    // corroded shell plates eaten full of holes, drifting around the vent: the armor it strips
    const spin = P(root, [0, 0, 44], "Spinner");
    for (let i = 0; i < 4; i++) {
      const an = i / 4 * 6.283 + 0.4, g = P(spin, [Math.cos(an) * 46, Math.sin(an) * 46, (i % 2) * 16]);
      g.quaternion.copy(rotQ([18, an / D2R, 22 * (i % 2 ? 1 : -1)]));
      S(g, "sphere", [0, 0, 0], [3, 19, 16], i % 2 ? [0.82, 0.42, 0.2] : [0.5, 0.52, 0.45]);
      S(g, "sphere", [1.4, -4, 2], [1.6, 5.5, 5], C.black); S(g, "sphere", [1.4, 5, -3], [1.6, 4, 4], C.black); S(g, "sphere", [1.4, 1, 5.4], [1.5, 2.6, 2.6], C.black);
      S(g, "sphere", [1.5, -1, -5], [2, 3, 3], 0, null, { material: acid }); S(g, "sphere", [1.7, 7, 3], [1.5, 2.2, 2.2], 0, null, { material: acid });
    }
    // preview smoke: black billows from each mouth, acid droplets falling
    const smokes = [];
    const addSmoke = (x, y, z, n, size, rise) => { for (let i = 0; i < n; i++) { const m = mat([0.05, 0.045, 0.06], { opacity: 0.7, unique: true }); smokes.push({ m, s: S(root, "sphere", [x, y, z], [size, size, size], 0, null, { material: m }), x, y, z, i, n, size, rise }); } };
    if (PORTING) {
      // five puffs up the main chimney, then three on the side ones: the game moves them
      const puff = (x, y, z, size) => { S(P(root, [x, y, z], "Puff"), "sphere", [0, 0, 0], [size, size, size], [0.06, 0.055, 0.07], null, { opacity: 0.62 }); };
      for (let i = 0; i < 5; i++) puff(-2, 0, mainTop, 22);
      puff(24, -17, sideA, 13); puff(24, -17, sideA, 13); puff(-23, 22, sideB, 11);
    } else { if (PORTING) {
      // five puffs up the main chimney, then three on the side ones: the game moves them
      const puff = (x, y, z, size) => { S(P(root, [x, y, z], "Puff"), "sphere", [0, 0, 0], [size, size, size], [0.06, 0.055, 0.07], null, { opacity: 0.62 }); };
      for (let i = 0; i < 5; i++) puff(-2, 0, mainTop, 22);
      puff(24, -17, sideA, 13); puff(24, -17, sideA, 13); puff(-23, 22, sideB, 11);
    } else { addSmoke(-2, 0, mainTop, 14, 16, 74); addSmoke(24, -17, sideA, 5, 9, 36); addSmoke(-23, 22, sideB, 4, 8, 28); } }
    const drops = []; for (let i = 0; i < 6; i++) drops.push(fx(S(root, "sphere", [0, 0, 0], [3, 3, 4.5], 0, null, { material: acid })));
    const ringM = mat([0.8, 1, 0.2], { emissive: [0.5, 0.75, 0.05], opacity: 0, unique: true });
    const ring = fx(pulseRing(root, 30, 40, 5, 4, ringM));
    const l = light(root, [-2, 0, mainTop], 0xff7a20, 0.9, 200);
    return (t) => {
      smokes.forEach(o => {
        const u = (t * 0.2 + o.i / o.n) % 1, wob = Math.sin(t * 1.2 + o.i * 2.1);
        o.s.position.set(o.x + wob * (2 + u * o.size * 0.7) + u * 5, o.y + Math.cos(t * 0.9 + o.i * 1.7) * (2 + u * o.size * 0.6), o.z + u * o.rise);
        o.s.scale.setScalar((o.size / 100) * (0.55 + u * 1.5));
        o.m.opacity = 0.78 * Math.min(1, u * 6) * (1 - u);
      });
      plumes.forEach((g, i) => { g.rotation.x = Math.sin(t * 1.5 + i) * 0.22; g.rotation.y = Math.cos(t * 1.2 + i * 1.3) * 0.22; });
      spin.rotation.z = -t * 40 * D2R;
      drops.forEach((d, i) => { const u = (t * 0.45 + i / 6) % 1, an = i * 1.05 + 0.5; d.position.set(Math.cos(an) * 16, Math.sin(an) * 16, 62 - u * 46); d.visible = u < 0.9; });
      const ph = (t % 3) / 3; ring.scale.set(0.45 + ph * 1.15, 0.45 + ph * 1.15, 1); ringM.opacity = 0.8 * (1 - ph);
      l.intensity = 0.75 + Math.sin(t * 7) * 0.2;
    };
  },
};

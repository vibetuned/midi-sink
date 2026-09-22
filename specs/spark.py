import numpy as np
import matplotlib.pyplot as plt

# -------------------------------------------------------------
# 1. Hamiltonian & Symplectic Vector Field
# -------------------------------------------------------------
def dH_dq(q, p, eps=0.25):
    """dH/dq = sin(q) + 2*eps*cos(2q)"""
    return np.sin(q) + 2.0 * eps * np.cos(2.0 * q)

def dH_dp(q, p):
    """dH/dp = p"""
    return p

def symplectic_step(q, p, dt):
    """Symplectic Euler update: Area-preserving map."""
    p_next = p - dt * dH_dq(q, p)
    q_next = q + dt * dH_dp(q, p_next)
    return q_next, p_next

# -------------------------------------------------------------
# 2. Fractal Spark Generator (Recursive Streamer Tree)
# -------------------------------------------------------------
def grow_spark(q0, p0, steps=80, dt=0.04, depth=0, max_depth=4, branch_prob=0.18):
    lines = []
    q, p = q0, p0
    trajectory = [(q, p)]
    
    for step in range(steps):
        # Symplectic drift
        q_det, p_det = symplectic_step(q, p, dt)
        
        # Stochastic dielectric breakdown perturbation
        noise_scale = 0.08 / (1.0 + 0.3 * depth)
        q = q_det + np.random.normal(0, noise_scale)
        p = p_det + np.random.normal(0, noise_scale)
        
        trajectory.append((q, p))
        
        # Branching condition based on local phase-space kinetic energy
        local_field_strength = np.hypot(dH_dq(q, p), dH_dp(q, p))
        dynamic_prob = branch_prob * (1.0 + 0.5 * local_field_strength)
        
        if depth < max_depth and np.random.rand() < dynamic_prob and step > 8:
            # Fork a child branch with transverse momentum kick
            p_fork = p + np.random.choice([-1, 1]) * (0.3 + 0.2 * np.random.rand())
            child_steps = int(steps * np.random.uniform(0.4, 0.75))
            child_lines = grow_spark(
                q, p_fork, 
                steps=child_steps, 
                dt=dt, 
                depth=depth + 1, 
                max_depth=max_depth, 
                branch_prob=branch_prob * 0.8
            )
            lines.extend(child_lines)
            
    lines.append((np.array(trajectory), depth))
    return lines

# -------------------------------------------------------------
# 3. Execution and Visual Styling
# -------------------------------------------------------------
np.random.seed(42)
spark_network = grow_spark(q0=-np.pi + 0.2, p0=1.8, steps=110, dt=0.045)

fig, ax = plt.subplots(figsize=(10, 6), facecolor="#030308")
ax.set_facecolor("#030308")

# Background Hamiltonian Phase Contours
q_grid = np.linspace(-4, 4, 300)
p_grid = np.linspace(-3, 3, 300)
Q, P = np.meshgrid(q_grid, p_grid)
H = 0.5 * P**2 - np.cos(Q) + 0.25 * np.sin(2.0 * Q)
ax.contour(Q, P, H, levels=25, colors="#152238", linewidths=0.6, alpha=0.6)

# Render Spark with Multi-Pass Glow
for trajectory, depth in spark_network:
    qs = trajectory[:, 0]
    ps = trajectory[:, 1]
    
    # Outer cyan/electric blue glow
    glow_width = max(0.5, (4.5 - depth * 0.9) * 2.2)
    ax.plot(qs, ps, color="#00e5ff", lw=glow_width, alpha=0.12, solid_capstyle="round")
    
    # Mid-layer neon
    mid_width = max(0.3, (3.5 - depth * 0.7))
    ax.plot(qs, ps, color="#76ffff", lw=mid_width, alpha=0.45, solid_capstyle="round")
    
    # White-hot central core
    core_width = max(0.2, (1.8 - depth * 0.35))
    ax.plot(qs, ps, color="#ffffff", lw=core_width, alpha=0.95, solid_capstyle="round")

ax.set_xlim(-3.8, 3.8)
ax.set_ylim(-2.8, 2.8)
ax.set_xlabel("Generalized Coordinate ($q$)", color="#8b9bb4", fontsize=11)
ax.set_ylabel("Canonical Momentum ($p$)", color="#8b9bb4", fontsize=11)
ax.tick_params(colors="#50637f")
for spine in ax.spines.values():
    spine.set_color("#1a2636")

plt.tight_layout()
plt.show()
# wm-motion-matcher
An Unreal Engine 4 plugin that implements "motion matching" for animated 3D characters.

Instead of tediously placing loops and transition animation clips in a carefully structured state machine, Motion Matching automatically chooses the best animation for your character controller. By creating a cache of information (pose data, velocity, etc.) pertaining to the animations and placing the variables in a multi-dimensional feature space, we can calculate the squared euclidean distance between our character's current pose (also in feature space) and a candidate from the cache. The closest candidate is then transitioned into via inertialization blending, reducing costly crossfades.
 
## Notes
It's not tested in its current form (I've been reorganizing and renaming nodes) and might not even compile in Unreal but 90% of the work is already done. Additional features are pending.


## Reference
- "Motion Matching and The Road to Next-Gen Animation", GDC 2016
- "Motion Matching in The Last of Us Part II", GDC 2021
- "Inertialization: High-Performance Animation Transitions in 'Gears of War'", GDC 2018

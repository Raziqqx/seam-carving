## Remove and repeat (item 1c)

```
carve_to_size(image, target_cols, target_rows):
    while image.cols > target_cols:                 # vertical seams first
        energy = energy map of the CURRENT image    # recomputed after every seam
        seam   = find one seam (DP | greedy | graph cut)
        image  = image without the seam pixels      # each row shifts left by one
    transpose the image and repeat for the rows     # horizontal seams
```

**Recomputing the energy.** Removing a seam puts pixels next to each other that were not
neighbours before, so the gradients near the seam change. `find_vertical_seam` therefore
computes the energy from the current image every time. A test replays the loop by hand and
checks that the library made exactly the same choices.

**Horizontal seams by transposing.** A horizontal seam of an image is a vertical seam of its
transpose (rows and columns swapped). So we never write a second version of any algorithm:
`remove_seams` transposes once, removes vertical seams, and transposes back. This halves the
code that can contain bugs, and a test checks
`horizontal(image) == transpose(vertical(transpose(image)))` for all three methods.

**Order of width and height.** When both change we remove all vertical seams first, then all
horizontal ones. Avidan and Shamir show that the optimal interleaving can be found with
another DP over (rows removed, columns removed); we did not implement it because it
multiplies the work by the number of seams and the visual difference is small.

**Seam insertion (enlarging).** To make the image $k$ columns wider we first find the $k$
seams that *would be removed*, by removing them from a scratch copy. Then, seam by seam, we
insert one new pixel to the right of every pixel of that seam (`insert_vertical_seam`, the
mirror image of `remove_vertical_seam`); the new pixel is the average of its two neighbours.
Finding the $k$ seams first matters: inserting the single best seam $k$ times would stretch
one stripe of the picture.

**Tracking original positions.** The image is carried through the whole resize together
with an *index map*: an integer image of the same size whose entry says which pixel of the
original image each current pixel is (`TrackedImage` in `src/carve.cpp`). Every seam is
removed from, or inserted into, both at the same time; pixels created by an insertion are
marked as having no original position. The index map is what lets us (a) tell which
original pixels an insertion must duplicate, and (b) draw every seam of both stages on the
original image, even when the width is reduced and the height enlarged in the same call
(bonus 7a).

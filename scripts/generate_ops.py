import random
import argparse
import os
import math
import sys

def generate_operations(
    n_elements,
    n_operations,
    find_ratio,
    sameset_ratio,
    contention_level,
    hot_element_index,
    extreme_contention,
    output_filename
):
    """
    Generates a file containing Union-Find operations (UNION=0, FIND=1, SAMESET=2).
    Supports uniform, focused (hot element), or extreme (elements 0 and 1 only) contention.

    Args:
        n_elements (int): The number of elements (0 to n_elements-1).
        n_operations (int): The total number of operations to generate.
        find_ratio (float): The desired ratio of FIND operations (0.0 to 1.0).
        sameset_ratio (float): The desired ratio of SAMESET operations *within*
                               the non-FIND operations (0.0 to 1.0).
        contention_level (float): Controls focus for 'focused' mode (0.0 low, 1.0 high). Ignored if extreme_contention is True.
        hot_element_index (int): Index for focused contention. Ignored if extreme_contention is True.
        extreme_contention (bool): If True, forces all operations onto elements 0 and 1.
        output_filename (str): The path to the output file.
    """
    # --- Input Validation ---
    if not (0.0 <= find_ratio <= 1.0):
        raise ValueError("find_ratio must be between 0.0 and 1.0")
    if not (0.0 <= sameset_ratio <= 1.0):
        raise ValueError("sameset_ratio must be between 0.0 and 1.0")
    if not extreme_contention and not (0.0 <= contention_level <= 1.0):
        raise ValueError("contention_level must be between 0.0 and 1.0 unless --extreme-contention is used")
    if n_elements <= 0:
        raise ValueError("n_elements must be positive")
    if extreme_contention and n_elements < 2:
        raise ValueError("n_elements must be at least 2 for --extreme-contention mode")
    if n_operations <= 0:
        raise ValueError("n_operations must be positive")
    if not extreme_contention and not (0 <= hot_element_index < n_elements):
        raise ValueError(f"hot_element_index ({hot_element_index}) must be between 0 and {n_elements-1} unless --extreme-contention is used")

    print(f"Generating {n_operations} operations for {n_elements} elements...")
    print(f"Target FIND ratio: {find_ratio:.2f}")
    print(f"Target SAMESET ratio (of non-FIND ops): {sameset_ratio:.2f}")

    # --- Contention Mode Setup ---
    hot_access_prob = 0.0 # Default
    select_element_func = None

    if extreme_contention:
        print("Contention Mode: Extreme (Operations focused exclusively on elements 0 and 1)")
        # No need for probability calculation or select_element function in this mode
    elif n_elements == 1:
        print("Note: n_elements is 1. Only FIND(0) operations are possible.", file=sys.stderr)
        find_ratio = 1.0 # Force all ops to be FIND
        hot_access_prob = 1.0 # Only one element to choose
        hot_element_index = 0
        print(f"Contention Level: N/A (n_elements=1)")
        def select_element_single():
             return 0
        select_element_func = select_element_single
    else:
        # Calculate Hot Element Access Probability for focused mode
        max_focus_prob = 0.95 # Using the 0.95 value discussed
        uniform_prob = 1.0 / n_elements
        hot_access_prob = contention_level * max_focus_prob + (1.0 - contention_level) * uniform_prob
        hot_access_prob = min(1.0, hot_access_prob) # Ensure probability is capped

        print(f"Contention Mode: Focused on element {hot_element_index}")
        print(f"  Contention Level: {contention_level:.2f}")
        print(f"  Probability of accessing element {hot_element_index} directly: {hot_access_prob:.4f}")

        # --- Helper function to select an element based on focused contention ---
        # Need to define it here so it captures the calculated hot_access_prob and hot_element_index
        def select_element_focused():
            if random.random() < hot_access_prob:
                return hot_element_index
            else:
                # Choose any element uniformly among all elements
                return random.randint(0, n_elements - 1)
        select_element_func = select_element_focused


    print(f"Output file: {output_filename}")

    # --- Ensure Output Directory Exists ---
    output_dir = os.path.dirname(output_filename)
    if output_dir and not os.path.exists(output_dir):
        try:
            os.makedirs(output_dir)
            print(f"Created directory: {output_dir}")
        except OSError as e:
            print(f"Error creating directory {output_dir}: {e}", file=sys.stderr)
            return # Cannot proceed if directory creation fails

    # --- Generate Operations ---
    try:
        with open(output_filename, 'w') as f:
            # Write header: <num_elements> <num_operations>
            f.write(f"{n_elements} {n_operations}\n")

            generated_ops_count = 0
            find_count = 0
            union_count = 0
            sameset_count = 0
            # Track accesses involving elements 0 or 1 in extreme mode, or the hot element otherwise
            relevant_element_accesses = 0
            relevant_element_indices = set([0, 1]) if extreme_contention else set([hot_element_index])

            for i in range(n_operations):
                op_type_val = -1 # Placeholder
                a = -1
                b = -1

                # Decide if FIND or non-FIND
                is_find_op = (random.random() < find_ratio) or (n_elements == 1)

                if extreme_contention:
                    # --- Extreme Contention Logic ---
                    if is_find_op:
                        op_type_val = 1
                        a = random.choice([0, 1]) # FIND(0) or FIND(1)
                        b = 0 # Ignored
                        find_count += 1
                        if a in relevant_element_indices: relevant_element_accesses += 1
                    else:
                        # UNION(0, 1) or SAMESET(0, 1)
                        a = 0
                        b = 1
                        if random.random() < sameset_ratio:
                            op_type_val = 2 # SAMESET
                            sameset_count += 1
                        else:
                            op_type_val = 0 # UNION
                            union_count += 1
                        # Both a and b are relevant
                        if a in relevant_element_indices: relevant_element_accesses += 1
                        if b in relevant_element_indices: relevant_element_accesses += 1

                else:
                    # --- Focused or Uniform Contention Logic ---
                    if is_find_op:
                        op_type_val = 1
                        a = select_element_func()
                        if a in relevant_element_indices:
                            relevant_element_accesses += 1
                        b = 0 # Ignored
                        find_count += 1
                    else:
                        # Select 'a'
                        a = select_element_func()
                        if a in relevant_element_indices:
                            relevant_element_accesses += 1

                        # Select 'b' ensuring b != a
                        while True:
                            b = select_element_func()
                            if a != b:
                                if b in relevant_element_indices:
                                    relevant_element_accesses += 1
                                break

                        # Decide if UNION or SAMESET based on sameset_ratio
                        if random.random() < sameset_ratio:
                            op_type_val = 2 # SAMESET
                            sameset_count += 1
                        else:
                            op_type_val = 0 # UNION
                            union_count += 1

                # Write operation: <type> <a> <b>
                f.write(f"{op_type_val} {a} {b}\n")
                generated_ops_count += 1

            # --- Print Summary ---
            print("-" * 30)
            print(f"Successfully generated {generated_ops_count} operations.")
            actual_find_ratio = find_count / generated_ops_count if generated_ops_count > 0 else 0
            non_find_count = union_count + sameset_count
            actual_sameset_ratio = sameset_count / non_find_count if non_find_count > 0 else 0 # Ratio within non-FIND ops
            total_element_accesses = find_count + 2 * non_find_count # Count accesses for a and b
            actual_relevant_access_ratio = relevant_element_accesses / total_element_accesses if total_element_accesses > 0 else 0

            print(f"Actual FIND operations:    {find_count:>10} ({actual_find_ratio:.4f})")
            print(f"Actual UNION operations:   {union_count:>10}")
            print(f"Actual SAMESET operations: {sameset_count:>10}")
            if non_find_count > 0:
                 print(f"  (SAMESET ratio of non-FIND: {actual_sameset_ratio:.4f})")

            if extreme_contention:
                 print(f"Total accesses involving elements 0 or 1: {relevant_element_accesses:>6} / {total_element_accesses} ({actual_relevant_access_ratio:.4f})")
            elif n_elements > 1:
                 print(f"Total accesses to hot element ({hot_element_index}): {relevant_element_accesses:>6} / {total_element_accesses} ({actual_relevant_access_ratio:.4f})")
            print("-" * 30)

    except IOError as e:
        print(f"Error writing to file {output_filename}: {e}", file=sys.stderr)
    except Exception as e:
        print(f"An unexpected error occurred during generation: {e}", file=sys.stderr)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Generate operation files for Union-Find benchmarking (UNION=0, FIND=1, SAMESET=2). Supports uniform, focused, or extreme contention.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter # Show defaults in help
    )

    parser.add_argument("n_elements", type=int, help="Number of elements (e.g., 10000).")
    parser.add_argument("n_operations", type=int, help="Number of operations (e.g., 100000).")
    parser.add_argument("output_file", type=str, help="Path to the output operations file (e.g., datasets/ops_10k_100k_c0.5_h0.txt).")
    parser.add_argument("--find-ratio", type=float, default=0.5,
                        help="Target ratio of FIND operations (0.0 to 1.0).")
    parser.add_argument("--sameset-ratio", type=float, default=0.1,
                        help="Target ratio of SAMESET operations *within the non-FIND* operations (0.0 to 1.0).")
    # Contention arguments - only one mode active at a time
    parser.add_argument("--contention-level", type=float, default=0.0,
                        help="Level of contention focus on the hot element (0.0=low/uniform, 1.0=high/focused). Ignored if --extreme-contention is used.")
    parser.add_argument("--hot-element", type=int, default=0,
                        help="Index of the element for focused contention. Ignored if --extreme-contention is used.")
    parser.add_argument("--extreme-contention", action="store_true",
                        help="Use extreme contention mode: all operations target only elements 0 and 1. Overrides --contention-level and --hot-element.")
    parser.add_argument("--seed", type=int, default=None,
                        help="Optional random seed for reproducibility.")

    args = parser.parse_args()

    # --- Set Seed ---
    if args.seed is not None:
        random.seed(args.seed)
        print(f"Using random seed: {args.seed}")

    # --- Run Generator ---
    try:
        # Validate hot_element index only if not in extreme mode
        if not args.extreme_contention and not (0 <= args.hot_element < args.n_elements):
            raise ValueError(f"--hot-element ({args.hot_element}) must be between 0 and n_elements-1 ({args.n_elements-1}) unless --extreme-contention is used.")
        # Validate n_elements for extreme mode
        if args.extreme_contention and args.n_elements < 2:
            raise ValueError(f"n_elements ({args.n_elements}) must be at least 2 for --extreme-contention mode.")

        generate_operations(
            args.n_elements,
            args.n_operations,
            args.find_ratio,
            args.sameset_ratio,
            args.contention_level,
            args.hot_element,
            args.extreme_contention, # Pass the flag
            args.output_file
        )
    except ValueError as e:
        print(f"Input Error: {e}", file=sys.stderr)
        sys.exit(1) # Exit with error code
    except Exception as e:
        print(f"An unexpected error occurred: {e}", file=sys.stderr)
        sys.exit(1) # Exit with error code

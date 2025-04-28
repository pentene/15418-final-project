import random
import argparse
import os
import math
import sys

def generate_operations(n_elements, n_operations, find_ratio, sameset_ratio, contention_level, hot_element_index, output_filename):
    """
    Generates a file containing Union-Find operations (UNION=0, FIND=1, SAMESET=2)
    using a focused contention model where one element is accessed more frequently.

    Args:
        n_elements (int): The number of elements (0 to n_elements-1).
        n_operations (int): The total number of operations to generate.
        find_ratio (float): The desired ratio of FIND operations (0.0 to 1.0).
        sameset_ratio (float): The desired ratio of SAMESET operations *within*
                               the non-FIND operations (0.0 to 1.0).
        contention_level (float): Controls the focus of operations (0.0 low, 1.0 high).
                                  0.0: Ops spread uniformly.
                                  1.0: Ops heavily focused on hot_element_index.
        hot_element_index (int): The index of the element to focus contention on.
        output_filename (str): The path to the output file.
    """
    # --- Input Validation ---
    if not (0.0 <= find_ratio <= 1.0):
        raise ValueError("find_ratio must be between 0.0 and 1.0")
    if not (0.0 <= sameset_ratio <= 1.0):
        raise ValueError("sameset_ratio must be between 0.0 and 1.0")
    if not (0.0 <= contention_level <= 1.0):
        raise ValueError("contention_level must be between 0.0 and 1.0")
    if n_elements <= 0:
        raise ValueError("n_elements must be positive")
    if n_operations <= 0:
        raise ValueError("n_operations must be positive")
    if not (0 <= hot_element_index < n_elements):
        raise ValueError(f"hot_element_index ({hot_element_index}) must be between 0 and {n_elements-1}")

    # --- Calculate Hot Element Access Probability ---
    if n_elements == 1:
        print("Note: n_elements is 1. Only FIND(0) operations are possible.", file=sys.stderr)
        find_ratio = 1.0 # Force all ops to be FIND
        hot_access_prob = 1.0 # Only one element to choose
    else:
        # Probability of choosing the hot element directly.
        # Scales from uniform probability (1/N) at contention 0
        # up to a high probability (e.g., 80%) at contention 1.
        # Adjust the 0.8 factor to control max focus.
        max_focus_prob = 0.8
        uniform_prob = 1.0 / n_elements
        hot_access_prob = contention_level * max_focus_prob + (1.0 - contention_level) * uniform_prob
        # Ensure probability is capped at 1.0 (shouldn't happen with max_focus_prob <= 1)
        hot_access_prob = min(1.0, hot_access_prob)

    print(f"Generating {n_operations} operations for {n_elements} elements...")
    print(f"Target FIND ratio: {find_ratio:.2f}")
    print(f"Target SAMESET ratio (of non-FIND ops): {sameset_ratio:.2f}")
    print(f"Contention Level: {contention_level:.2f} -> Focused on element {hot_element_index}")
    print(f"  Probability of accessing element {hot_element_index} directly: {hot_access_prob:.4f}")
    print(f"Output file: {output_filename}")

    # --- Helper function to select an element based on contention ---
    def select_element():
        if n_elements == 1:
            return 0
        if random.random() < hot_access_prob:
            return hot_element_index
        else:
            # Choose any element uniformly *other than* the hot one?
            # Or choose any element uniformly including the hot one?
            # Let's choose uniformly from all elements for simplicity.
            # The high hot_access_prob already creates the focus.
            return random.randint(0, n_elements - 1)

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
            hot_element_accesses = 0 # Track accesses to the hot element

            for i in range(n_operations):
                op_type_val = -1 # Placeholder
                a = -1
                b = -1

                # Decide if FIND or non-FIND
                is_find_op = (random.random() < find_ratio) or (n_elements == 1)

                if is_find_op:
                    # --- Generate FIND operation ---
                    op_type_val = 1
                    a = select_element()
                    if a == hot_element_index:
                        hot_element_accesses += 1
                    # 'b' is ignored for FIND, set to 0 for consistency
                    b = 0
                    find_count += 1
                else:
                    # --- Generate non-FIND (UNION or SAMESET) ---
                    # This block only runs if n_elements > 1
                    op_type_val = 0 # Default to UNION

                    # Select 'a'
                    a = select_element()
                    if a == hot_element_index:
                        hot_element_accesses += 1

                    # Select 'b' ensuring b != a
                    while True:
                        b = select_element()
                        if a != b:
                            if b == hot_element_index:
                                hot_element_accesses += 1
                            break
                        # If a == b, the access to b doesn't count yet,
                        # as it wasn't a valid selection for the operation.

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
            actual_hot_access_ratio = hot_element_accesses / total_element_accesses if total_element_accesses > 0 else 0

            print(f"Actual FIND operations:    {find_count:>10} ({actual_find_ratio:.4f})")
            print(f"Actual UNION operations:   {union_count:>10}")
            print(f"Actual SAMESET operations: {sameset_count:>10}")
            if non_find_count > 0:
                 print(f"  (SAMESET ratio of non-FIND: {actual_sameset_ratio:.4f})")
            print(f"Total accesses to hot element ({hot_element_index}): {hot_element_accesses:>6} / {total_element_accesses} ({actual_hot_access_ratio:.4f})")
            print("-" * 30)

    except IOError as e:
        print(f"Error writing to file {output_filename}: {e}", file=sys.stderr)
    except Exception as e:
        print(f"An unexpected error occurred during generation: {e}", file=sys.stderr)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Generate operation files for Union-Find benchmarking (UNION=0, FIND=1, SAMESET=2) using focused contention on a specific element.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter # Show defaults in help
    )

    parser.add_argument("n_elements", type=int, help="Number of elements (e.g., 10000).")
    parser.add_argument("n_operations", type=int, help="Number of operations (e.g., 100000).")
    parser.add_argument("output_file", type=str, help="Path to the output operations file (e.g., datasets/ops_10k_100k_c0.5_h0.txt).")
    parser.add_argument("--find-ratio", type=float, default=0.5,
                        help="Target ratio of FIND operations (0.0 to 1.0).")
    parser.add_argument("--sameset-ratio", type=float, default=0.1,
                        help="Target ratio of SAMESET operations *within the non-FIND* operations (0.0 to 1.0).")
    parser.add_argument("--contention-level", type=float, default=0.0,
                        help="Level of contention focus on the hot element (0.0=low/uniform, 1.0=high/focused).")
    parser.add_argument("--hot-element", type=int, default=0,
                        help="Index of the element to focus contention on.")
    parser.add_argument("--seed", type=int, default=None,
                        help="Optional random seed for reproducibility.")

    args = parser.parse_args()

    # --- Set Seed ---
    if args.seed is not None:
        random.seed(args.seed)
        print(f"Using random seed: {args.seed}")

    # --- Run Generator ---
    try:
        # Validate hot_element index *after* n_elements is parsed
        if not (0 <= args.hot_element < args.n_elements):
             raise ValueError(f"--hot-element ({args.hot_element}) must be between 0 and n_elements-1 ({args.n_elements-1})")

        generate_operations(
            args.n_elements,
            args.n_operations,
            args.find_ratio,
            args.sameset_ratio,
            args.contention_level,
            args.hot_element, # Pass the hot element index
            args.output_file
        )
    except ValueError as e:
        print(f"Input Error: {e}", file=sys.stderr)
        sys.exit(1) # Exit with error code
    except Exception as e:
        print(f"An unexpected error occurred: {e}", file=sys.stderr)
        sys.exit(1) # Exit with error code

